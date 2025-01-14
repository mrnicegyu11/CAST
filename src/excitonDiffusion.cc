#include "excitonDiffusion.h"

coords::Cartesian_Point exciD::avgDimCoM(coords::Cartesian_Point posA, coords::Cartesian_Point posB)//to calculate average of 2 cart coordinates, for average com of a dimer
{
  double x = (posA.x() + posB.x()) / 2;
  double y = (posA.y() + posB.y()) / 2;
  double z = (posA.z() + posB.z()) / 2;

  coords::Cartesian_Point ret;
  ret.x() = x;
  ret.y() = y;
  ret.z() = z;

  return ret;
}

double exciD::length(coords::Cartesian_Point pointA, coords::Cartesian_Point pointB)//function to calculate the length of a vector between two cartesian points
{
  double len = sqrt((pointA.x() - pointB.x()) * (pointA.x() - pointB.x()) + (pointA.y() - pointB.y()) * (pointA.y() - pointB.y()) 
                 + (pointA.z() - pointB.z()) * (pointA.z() - pointB.z()));
  return len;
}

double exciD::marcus(double coupling, double drivingF, double reorganisation)
{
  double marc;
  double prefactor = (coupling * coupling) / exciD::h_quer * sqrt(M_PI / (reorganisation * exciD::boltzmann_const * 298));
  double exponent = exp(-(reorganisation + drivingF) * (reorganisation + drivingF) / (4 * exciD::boltzmann_const * 298 * reorganisation));
  //marc = (coupling * coupling) / exciD::h_quer * sqrt(M_PI / (reorganisation * exciD::boltzmann_const * 298)) * 
  //       exp(-(reorganisation + drivingF) * (reorganisation + drivingF) / (4 * exciD::boltzmann_const * 298 * reorganisation));//replace 298 by user defined temperatur?
  marc = prefactor * exponent;
  return marc;
}

double exciD::coulomb(coords::Cartesian_Point aktPos, coords::Cartesian_Point targetPos, double e_relative)
{
  double e_0 = 8.854187e-12;
  double elementar = 1.60217662e-19;
  double coulomb = -elementar / (4 * M_PI * e_0 * e_relative * length(aktPos, targetPos) * 1e-10);
  return coulomb;
}

coords::Cartesian_Point exciD::structCenter(coords::Representation_3D com)//calculate average position of the given positions (more than two)
{
  coords::Cartesian_Point avg(0.0, 0.0, 0.0);
  for (std::size_t i = 0u; i < com.size(); i++)
  {
    avg += com[i];
  }
  avg /= com.size();
  return avg;
}

coords::Cartesian_Point exciD::min(coords::Representation_3D coords)
{
  coords::Cartesian_Point min;

  min = coords[0];//initialize with coordinates of first point

  for (std::size_t i = 1u; i < coords.size(); i++)
  {
    if (coords[i].x() < min.x())
    {
      min.x() = coords[i].x();
    }

    if (coords[i].y() < min.y())
    {
      min.y() = coords[i].y();
    }

    if (coords[i].z() < min.z())
    {
      min.z() = coords[i].z();
    }
  }
  return min;
}

coords::Cartesian_Point exciD::max(coords::Representation_3D coords)
{
  coords::Cartesian_Point max;
  max = coords[0];// initialize witzh coordinates of first point

  for (std::size_t i = 1u; i < coords.size(); i++)
  {
    if (coords[i].x() > max.x())
    {
      max.x() = coords[i].x();
    }

    if (coords[i].y() > max.y())
    {
      max.y() = coords[i].y();
    }

    if (coords[i].z() > max.z())
    {
      max.z() = coords[i].z();
    }
  }
  return max;
}

void exciD::dimexc(std::string masscenters, std::string couplings, std::size_t pscnumber, 
                   int nscnumber, char interfaceorientation, double startingPscaling, std::size_t nbrStatingpoins) {
  try {
	
	std::string couplingsname;
    double reorganisationsenergie_exciton = Config::get().exbreak.ReorgE_exc;//noch extra variablen in config.h und config.cc einfügen
    double reorganisationsenergie_charge = Config::get().exbreak.ReorgE_ch;
    double reorganisationsenergie_nSC = Config::get().exbreak.ReorgE_nSC;
    double reorganisationsenergie_ct = Config::get().exbreak.ReorgE_ct;
    double reorganisationsenergie_rek = Config::get().exbreak.ReorgE_rek;
    double triebkraft_ct = Config::get().exbreak.ct_triebkraft;
    double oszillatorstrength = Config::get().exbreak.oscillatorstrength;
    double wellenzahl = Config::get().exbreak.wellenzahl;
    double k_rad = wellenzahl * wellenzahl * oszillatorstrength; // fluoreszenz
    double chsepscaling = Config::get().exbreak.chsepscaling;

    char plane = interfaceorientation;//don't forget to replace by userinput
    
    couplingsname = couplings;


    std::ifstream comf;
    std::ifstream coupf;
    std::size_t numbermon;
    std::vector<std::size_t> startPind, viablePartners, h_viablePartners;
    std::vector<exciD::Partners> partnerConnections, h_partnerConnections;
    coords::Representation_3D com, pSCcom, nSCcom, averagePandN;
    coords::Cartesian_Point avg;
    exciD::Exciton excPos;

    std::ofstream run;
    if (Config::get().general.verbosity >= 4u)
      run.open("debug_xb_dimer_log.txt");

    comf.open(masscenters);
    comf >> numbermon;
    //comf.ignore(256, '\n');//ignore all till newline 

    comf.ignore(std::numeric_limits < std::streamsize >::max(), '\n');

    coords::Cartesian_Point tmp;
    std::string unnecessaryindex;
    std::string line;

    //read centers of mass
    for (std::size_t i = 0; i < numbermon; i++)
    {
      comf >> unnecessaryindex >> tmp;
      com.push_back(tmp);
    }

    //check if the number of masscenters matches the expected value
    std::cout << numbermon << " " << com.size() << '\n';
    if (numbermon != com.size())
    {
      throw std::logic_error("Unclear number of centers of mass.");
    }

    coupf.open(couplingsname);

    std::string tmpcount;
    std::size_t tmpA(0), tmpB(0);
    double tmpC(.0), tmpD(.0);
    exciD::Couplings tmpE;
    std::vector<exciD::Couplings> excCoup;
    double avgCoup{ 0.0 };
    

std::cout << "Couplings are read from: " << couplings << '\n';

    //read monomer indices of dimers and corresponding exciton-coupling and save in vector of couplings
    while (!coupf.eof())
    {
      std::getline(coupf, line);
      std::istringstream iss(line);
      std::size_t numberofelements(0);

      bool writeC = false;

      while (!iss.eof())
      {
        iss >> tmpcount;
        numberofelements++;
      }
      std::istringstream iss1(line);
      if (numberofelements < 3)
      { /*to ensure the last line isn't written two times*/}
      else if (numberofelements < 4 )
      {
        iss1 >> tmpA >> tmpB >> tmpC;
        tmpD = 0.0;
        writeC = true;
      }
      else if (numberofelements < 6 )
      {
        iss1 >> tmpA >> tmpB >> tmpC >> tmpD;
        writeC = true;
      }
      else
      {
        throw std::logic_error("Unexpected number of elements in couplingsfile.");
      }

      if (writeC == true)
      {
        exciD::Couplings tmpE(tmpA, tmpB, tmpC, tmpD);
        excCoup.push_back(tmpE);
      }

    }
    

    std::cout << "Number of dimer-pairs " << excCoup.size() << '\n';

    //calculate average centers of mass for relevant dimers and add to struct
    for (std::size_t i = 0u; i < excCoup.size(); i++)
    {
      excCoup[i].position = (exciD::avgDimCoM(com[excCoup[i].monA - 1], com[excCoup[i].monB - 1]));
    }

    for (size_t i = 0; i < pscnumber; i++)
    {
      pSCcom.push_back(com[i]);
    }

    for (size_t i = pscnumber; i < com.size(); i++)
    {
      nSCcom.push_back(com[i]);
    }

    avg = exciD::structCenter(com);//average position for all monomers

    //pSCavg
    averagePandN.push_back(exciD::structCenter(pSCcom));//average position of pSCs

    //nSCavg 
    averagePandN.push_back(exciD::structCenter(nSCcom));//average position of nSCs

    coords::Cartesian_Point minV = min(com);//lowest coordinatesin structure
    coords::Cartesian_Point maxV = max(com);//highest coordinates in structure

    std::random_device rd; //prepare rng
  /*  std::default_random_engine engine(rd());*/ //new generated for every step
    std::normal_distribution<double> distributionN(0.0, 0.068584577);
    std::uniform_real_distribution<double> distributionR(0, 1); //beispiel für rng var: double rng = distributionN(engine);

    //choose startingpoints
    for (;startPind.size() < nbrStatingpoins;)
    {
      switch (plane)
      {
      case 'x':

        if (averagePandN[0].x() > avg.x())
        {
          for (std::size_t i = 0u; i < excCoup.size(); i++)
          {
            if (excCoup[i].position.x() - avg.x() > startingPscaling * (maxV.x() - avg.x()))
            {
              startPind.push_back(i);
            }
          }
        }
        else
        {
          for (std::size_t i = 0u; i < excCoup.size(); i++)
          {
            if (excCoup[i].position.x() - avg.x() < startingPscaling * (minV.x() - avg.x()))
            {
              startPind.push_back(i);
            }
          }
        }

        break;

      case 'y':

        if (averagePandN[0].y() > avg.y())
        {
          for (std::size_t i = 0u; i < excCoup.size(); i++)
          {
            if (excCoup[i].position.y() - avg.y() > startingPscaling * (maxV.y() - avg.y()))
            {
              startPind.push_back(i);
            }
          }
        }
        else
        {
          for (std::size_t i = 0u; i < excCoup.size(); i++)
          {
            if (excCoup[i].position.y() - avg.y() < startingPscaling * (minV.y() - avg.y()))
            {
              startPind.push_back(i);
            }
          }
        }

        break;

      case 'z':

        if (averagePandN[0].z() > avg.z())
        {
          for (std::size_t i = 0u; i < excCoup.size(); i++)
          {
            if (excCoup[i].position.z() - avg.z() > startingPscaling * (maxV.z() - avg.z()))
            {
              startPind.push_back(i);
            }
          }
        }
        else
        {
          for (std::size_t i = 0u; i < excCoup.size(); i++)
          {
            if (excCoup[i].position.z() - avg.z() < startingPscaling * (minV.z() - avg.z()))
            {
              startPind.push_back(i);
            }
          }
        }

        break;
      }

      if (startPind.size() < nbrStatingpoins)
      {
        startingPscaling -= 0.01;
        startPind.clear(); //To ensure no startingpoint is used more than once

      }
    }

    std::cout << "Used Startingpoint scaling factor: " << startingPscaling << '\n';
    std::cout << "Number of Startingpoints: : " << startPind.size() << '\n';
    std::cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << '\n';

    if (startPind.size() == 0)
    {
      throw std::logic_error("No Points to start the simulation were found.");
    }

    //loop for writing starting points
    std::ofstream startPout;
    startPout.open("Startingpoints.txt");

    for (std::size_t i = 0u; i < startPind.size(); i++)
    {
      startPout << "Startingpoint " << i << ": " << startPind[i] << " Monomer A " << excCoup[startPind[i]].monA << " Monomer B " << excCoup[startPind[i]].monB << '\n';
    }
    startPout.close();

    //vectors for counting the finishing states of the simulation
    std::vector <int> trapped(startPind.size(), 0);//vector for counting the trapped excitons unable to reach the interface from each startingpoint
    std::vector <int> ex_diss(startPind.size(), 0);
    std::vector <int> radiating(startPind.size(), 0);
    std::vector <int> rekombined(startPind.size(), 0);
    std::vector <int> ch_separation(startPind.size(), 0);

    //vectors to keep time/velocities for different startingpoints and tries
    std::vector <std::vector<double>> time_ch(startPind.size(), std::vector<double>(100, 0.)), 
                                      time_ex(startPind.size(), std::vector<double>(100, 0.)),
                                      vel_ch(startPind.size(), std::vector<double>(100, 0.)),
                                      vel_ex(startPind.size(), std::vector<double>(100, 0.));

    //double time(0.0), time_p(0.0), time_n(0.0);
    int const nbrof_tries = 100;
    //int const nbrof_steps = 2 * startPind.size() + 400;//old, stepnumber is now userdefined
    int const nbrof_steps = Config::get().exbreak.numberofsteps;


    //loop over all startingpoints 
    for (std::size_t i = 0u; i < startPind.size(); i++)
    {
      //loop ensures to start 100 times from every startingpoint
      for (std::size_t j = 0u; j < nbrof_tries; j++)//don't forget to set to 100 when all works fine
      {
        double time(0.0), time_p(0.0), time_n(0.0);
        bool check_maxSteps = true;

        //startPind[i] is the startingpoint for the actual simulation, excPos is used to keep track of the position of the exciton during simulation
        excPos.location = startPind[i];

        if (i == 0 && j < 2)
        {
          run << "Startingpoint " << i << ": " << startPind[i] << " Monomer A: " << excCoup[startPind[i]].monA
            << " Monomer B: " << excCoup[startPind[i]].monB << " Coupling: " << excCoup[excPos.location].coupling << '\n';
        }

        for (int h = 0; h < nbrof_steps; h++)//steps in each try 
        {
          std::mt19937  engine(rd());

          coords::Cartesian_Point sepLocation;

          if (i == 0 && j < 2)
          {
            run << "Exciton Position: " << excPos.location << '\n';
          }

          //loop over all dimerpairs for viable partners
          for (std::size_t k = 0; k < excCoup.size(); k++)
          {

            //in this if-cause the viable partners to the actual location are determined and 
            //the dimer pairs necessary to calculate the average couplings between the dimers are determined.
            if (excPos.location != k)//skip k if k is the index of the dimer where the exciton is at the moment
            {
              //ensure monomers at excCoup[excPos] are not part of excCoup[k]
              if ((excCoup[excPos.location].monA != excCoup[k].monA) && 
                  (excCoup[excPos.location].monA != excCoup[k].monB) && 
                  (excCoup[excPos.location].monB != excCoup[k].monA) && 
                  (excCoup[excPos.location].monB != excCoup[k].monB))
              {
                //check if excCoup[k] is close enough to excCoup[excPos]
                if (exciD::length(excCoup[excPos.location].position, excCoup[k].position) < 35.0)
                {
                  viablePartners.push_back(k);
                }
              }
            }// #if(excPos != k)

            if (excPos.state == 'c')
            {
              //same purpose as above but for location of possible second particle
              if (excPos.h_location != k)
              {
                if ((excCoup[excPos.h_location].monA != excCoup[k].monA) && 
                    (excCoup[excPos.h_location].monA != excCoup[k].monB) && 
                    (excCoup[excPos.h_location].monB != excCoup[k].monA) && 
                    (excCoup[excPos.h_location].monB != excCoup[k].monB))
                {
                  //check if excCoup[k] is close enough to excCoup[excPos]
                  if (exciD::length(excCoup[excPos.h_location].position, excCoup[k].position) < 35.0)
                  {
                    h_viablePartners.push_back(k);
                  }
                }
              }
            }
          }//dertermining of viable partners k

          //check if couplings of monomers in excPos exist to viable partner monomers, if not set value to zero
          for (std::size_t m = 0u; m < viablePartners.size(); m++)//loop over viable partners to find couplings between monomers in current posirtion and viable partners
          {
            std::vector<std::size_t> tmpG;
            if (i == 0 && j < 2)
            {
              run << "  Partners: " << excCoup[viablePartners[m]].monA << " " << excCoup[viablePartners[m]].monB << " " << excCoup[viablePartners[m]].coupling << '\n';
            }

            for (std::size_t l = 0u; l < excCoup.size(); l++)//loop over all dimerpairs which have couplings
            {
              if (excCoup[excPos.location].monA == excCoup[l].monA || 
                  excCoup[excPos.location].monA == excCoup[l].monB)//look if monomer A of the current location is part of the viewed Dimer
              {
                if (excCoup[excPos.location].monB != excCoup[l].monA &&
                    excCoup[excPos.location].monB != excCoup[l].monB)//ensure the other monomer is not also part of the viewed dimer
                {
                  if (excCoup[viablePartners[m]].monA == excCoup[l].monA ||
                      excCoup[viablePartners[m]].monA == excCoup[l].monB || 
                      excCoup[viablePartners[m]].monB == excCoup[l].monA || 
                      excCoup[viablePartners[m]].monB == excCoup[l].monB)//look if monomer of viablöe Parner is part of the viewed dimer
                  {
                    tmpG.push_back(l);
                  }
                }
              }
              else if (excCoup[excPos.location].monB == excCoup[l].monA || 
                       excCoup[excPos.location].monB == excCoup[l].monB)//look if monomer B of the current location is part of the viewed Dimer if monomer A is not
              {
                if (excCoup[excPos.location].monA != excCoup[l].monA && 
                    excCoup[excPos.location].monA != excCoup[l].monB)//ensure the other monomer is not also part of the viewed dimer
                {
                  if (excCoup[viablePartners[m]].monA == excCoup[l].monA || 
                      excCoup[viablePartners[m]].monA == excCoup[l].monB || 
                      excCoup[viablePartners[m]].monB == excCoup[l].monA || 
                      excCoup[viablePartners[m]].monB == excCoup[l].monB)//look if monomer of viablöe Parner is part of the viewed dimer
                  {
                    tmpG.push_back(l);
                  }
                }
              }
            }// l
            exciD::Partners tmpH(viablePartners[m], tmpG);
            partnerConnections.push_back(tmpH);
          } //m

          if (excPos.state == 'c')//hole movement only of interesst if simulation of charges is done (steate=c)
          {
            for (std::size_t m = 0u; m < h_viablePartners.size(); m++)//loop over viable partners to find couplings between monomers in current posirtion and viable partners
            {
              std::vector<std::size_t> tmpG;
              if (i == 0 && j < 2)
              {
                run << "  h_Partners: " << excCoup[h_viablePartners[m]].monA << " " << excCoup[h_viablePartners[m]].monB << " " << excCoup[h_viablePartners[m]].coupling << '\n';
              }

              for (std::size_t l = 0u; l < excCoup.size(); l++)//loop over all dimerpairs which have couplings
              {
                if (excCoup[excPos.h_location].monA == excCoup[l].monA || 
                    excCoup[excPos.h_location].monA == excCoup[l].monB)//look if monomer A of the current location is part of the viewed Dimer
                {
                  if (excCoup[excPos.h_location].monB != excCoup[l].monA && 
                      excCoup[excPos.h_location].monB != excCoup[l].monB)//ensure the other monomer is not also part of the viewed dimer
                  {
                    if (excCoup[h_viablePartners[m]].monA == excCoup[l].monA || 
                        excCoup[h_viablePartners[m]].monA == excCoup[l].monB || 
                        excCoup[h_viablePartners[m]].monB == excCoup[l].monA || 
                        excCoup[h_viablePartners[m]].monB == excCoup[l].monB)//look if monomer of viable Parner is part of the viewed dimer
                    {
                      tmpG.push_back(l);
                    }
                  }
                }
                else if (excCoup[excPos.h_location].monB == excCoup[l].monA || 
                         excCoup[excPos.h_location].monB == excCoup[l].monB)//look if monomer B of the current location is part of the viewed Dimer if monomer A is not
                {
                  if (excCoup[excPos.h_location].monA != excCoup[l].monA && 
                      excCoup[excPos.h_location].monA != excCoup[l].monB)//ensure the other monomer is not also part of the viewed dimer
                  {
                    if (excCoup[h_viablePartners[m]].monA == excCoup[l].monA || 
                        excCoup[h_viablePartners[m]].monA == excCoup[l].monB || 
                        excCoup[h_viablePartners[m]].monB == excCoup[l].monA || 
                        excCoup[h_viablePartners[m]].monB == excCoup[l].monB)//look if monomer of viablöe Parner is part of the viewed dimer
                    {
                      tmpG.push_back(l);
                    }
                  }
                }
              }
              exciD::Partners tmpH(h_viablePartners[m], tmpG);
              h_partnerConnections.push_back(tmpH);
            }
          }

          //calculate avgCouplings & avgsecCouplings
          for (std::size_t n = 0u; n < partnerConnections.size(); n++)
          {
            partnerConnections[n].avgCoup = 0.0;//prevent visiting undefined behaviour land
            partnerConnections[n].avgsecCoup = 0.0;

            for (std::size_t o = 0u; o < partnerConnections[n].connect.size(); o++)
            {
              partnerConnections[n].avgCoup += excCoup[partnerConnections[n].connect[o]].coupling;

              //if (excCoup[partnerConnections[n].connect[o]].seccoupling != 0.0)//if a opair has a second coupling its value is addet to avgsecCoup
              //{
              partnerConnections[n].avgsecCoup += excCoup[partnerConnections[n].connect[o]].seccoupling;
             /* }*/
            }// o
            //partnerConnections[w].avgCoup /= partnerConnections[w].connect.size();
            partnerConnections[n].avgCoup /= 4; //assuming not added couplings are zero

            if (partnerConnections[n].avgsecCoup != 0.0)//not every pair has a second coupling so bevoreS dividing its existence is checked
            {
              partnerConnections[n].avgsecCoup /= 4;
            }
          }// n

          if (excPos.state == 'c')//hole movement only of interest if simulation of charges is done (steate=c)
          {
            for (std::size_t n = 0u; n < h_partnerConnections.size(); n++)
            {
                h_partnerConnections[n].avgCoup = 0.0;//prevent visiting undefined behaviour land
                h_partnerConnections[n].avgsecCoup = 0.0;

              for (std::size_t o = 0u; o < h_partnerConnections[n].connect.size(); o++)
              {
                h_partnerConnections[n].avgCoup += excCoup[h_partnerConnections[n].connect[o]].coupling;

                //if (excCoup[h_partnerConnections[n].connect[o]].seccoupling != 0.0)//if a pair has a second coupling its value is addet to avgsecCoup
                //{
                h_partnerConnections[n].avgsecCoup += excCoup[h_partnerConnections[n].connect[o]].seccoupling;
                /*}*/
              }
              h_partnerConnections[n].avgCoup /= 4; //assuming not added couplings are zero

              if (h_partnerConnections[n].avgsecCoup != 0.0)//not every pair has a second coupling so bevore dividing its existence is checked
              {
                h_partnerConnections[n].avgsecCoup /= 4;
              }
            }
          }

          //writing loop for calculated avg Couplings
          if (i == 0 && j < 2)
          {
            for (std::size_t n = 0u; n < partnerConnections.size(); n++)
            {
              run << "PartnerIndex: " << partnerConnections[n].partnerIndex << " Monomers: " << excCoup[partnerConnections[n].partnerIndex].monA << " "
                << excCoup[partnerConnections[n].partnerIndex].monB << '\n';
              for (std::size_t o = 0u; o < partnerConnections[n].connect.size(); o++)
              {
                run << " ConnectorIndex: " << partnerConnections[n].connect[o] << " Monomers: " << excCoup[partnerConnections[n].connect[o]].monA << " "
                  << excCoup[partnerConnections[n].connect[o]].monB << " Coupling: " << excCoup[partnerConnections[n].connect[o]].coupling << " |"
                  << " secCoupling: " << excCoup[partnerConnections[n].connect[o]].seccoupling << " |" << '\n';
              } //o
              run << " Average Coupling: " << partnerConnections[n].avgCoup << " |" << "Average secCoupling: " << partnerConnections[n].avgsecCoup << '\n';
            }// n

            if (excPos.state == 'c')//hole movement only of uinteresst if simulation of charges is done (steate=c
            {
              for (std::size_t n = 0u; n < h_partnerConnections.size(); n++)
              {
                run << "h_PartnerIndex: " << h_partnerConnections[n].partnerIndex << " Monomers: " << excCoup[h_partnerConnections[n].partnerIndex].monA << " "
                  << excCoup[h_partnerConnections[n].partnerIndex].monB << '\n';
                for (std::size_t o = 0u; o < h_partnerConnections[n].connect.size(); o++)
                {
                  run << " Monomers: " << excCoup[h_partnerConnections[n].connect[o]].monA
                    << " h_ConnectorIndex: " << h_partnerConnections[n].connect[o]
                    << " " << excCoup[h_partnerConnections[n].connect[o]].monB
                    << " h_Coupling: " << excCoup[h_partnerConnections[n].connect[o]].coupling << " |" << " h_Coupling: "
                    << excCoup[h_partnerConnections[n].connect[o]].seccoupling << " |" << '\n';
                } //o
                run << " Average Coupling: " << h_partnerConnections[n].avgCoup << " |" << "Average secCoupling: " << h_partnerConnections[n].avgsecCoup << '\n';
              }// n
            }


            //algorithm for exciton movement starts here all before was preparation to know where movement to is possible

            run << "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" << '\n';
          }

          double rate_sum(0.0), rateFul_sum(0.0), coulombenergy, rate_KMC(0.0)/*, tmp_ratesum(0.0)*/;
          std::vector <double> raten;//used for exciton and electron rates
          std::vector <double> raten_hole;//used for hole rates
          double random_normal, random_normal1;
        //bool heterodimer(false);
          random_normal1 = distributionN(engine);
          //EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE
          if (excPos.state == 'e')//Exciton state
          {
            for (std::size_t p = 0u; p < partnerConnections.size(); p++)
            {
              if (excCoup[partnerConnections[p].partnerIndex].monA <= pscnumber && 
                  excCoup[partnerConnections[p].partnerIndex].monB <= pscnumber)//only for homo p-type SC due to reorganisation energies
              {
                random_normal = distributionN(engine);//generating normal distributed random number

                rate_sum += marcus(partnerConnections[p].avgsecCoup, (random_normal - random_normal1), reorganisationsenergie_exciton);//rate for homoPSCpartner
              }
              else if (excCoup[partnerConnections[p].partnerIndex].monA > pscnumber && excCoup[partnerConnections[p].partnerIndex].monB > pscnumber)
              {//CT only if BOTH patrner molecules are n-type SC
                random_normal = distributionN(engine);//generating normal distributed random number

                coulombenergy = coulomb(excCoup[excPos.location].position, excCoup[partnerConnections[p].partnerIndex].position, 1);
              //rate for heteroPSCpartner
                double singlerate =  marcus(partnerConnections[p].avgCoup, (random_normal - random_normal1) + coulombenergy + triebkraft_ct, reorganisationsenergie_ct);
                rate_sum += singlerate;
              }
              else//to prevent heterodimersfrom participating as exciton location HOW TO HANDLE HETERO DIMERS? CT OR EXCITONDIFFUSION?
              {
                //tmp_ratesum = rate_sum;
                rate_sum += 0.0;
                //heterodimer = true;
              }
              raten.push_back(rate_sum);

              if (i == 0 && j < 2)
              {
                run << "Partner: " << partnerConnections[p].partnerIndex << " Rates: " << rate_sum << '\n';
              }

              //if (heterodimer) //set rate_sum back to previous value
              //{
              //  heterodimer = false;
              //  rate_sum = tmp_ratesum;
              //}
            }// p
            rate_sum += k_rad;//accounting for fluorescence

            double random_real = distributionR(engine);

            rate_KMC = random_real * rate_sum;

            if (i == 0 && j < 2)
            {
              run << "Fluorescence Rate: " << k_rad << '\n';
              run << "Ratensumme: " << rate_sum << '\n';
              run << "KMC-Rate: " << rate_KMC << '\n';
            }

            //calculate time needed for step
            time += (1 / rate_sum);

            //trapping
            random_real = distributionR(engine);
            if (random_real * (900e-1 + 1 / rate_sum) > (900e-1))
            {
              if (i == 0 && j < 2)
              {
                run << "trapped" << '\n';
              }
              trapped[i]++;
              excPos.state = 't';
              viablePartners.clear();//empties vector containing possible partners for step so it can be reused in next try
              partnerConnections.clear();
              break;
            }

            //if (viablePartners.size() == 0)//if no partners are found the kmc point for radiating decay must be reached
            //{
            //  viablePartners.push_back(0);
            //  raten.push_back(0.0);//so the vector does not leave its reange in the following loop
            //}

            if (viablePartners.size() == 0)//if no partners are found fluorescence rate shoud stil be appied, so ensureing here check loop is entered
            {
              viablePartners.push_back(0);
              raten.push_back(0);
            }

            for (std::size_t q = 0u; q < viablePartners.size(); q++)
            {
              if (raten[q] >= rate_KMC)
              {
                if (excCoup[viablePartners[q]].monA > pscnumber && excCoup[viablePartners[q]].monB > pscnumber)
                {
                  excPos.h_location = excPos.location; //set hole position to former exciton position for charge separation
                  excPos.h_location_lastS = excPos.h_location;              
                }

                excPos.location_lastS = excPos.location;
                excPos.location = viablePartners[q];//after chargeseparation excPos.location tracks electron position.

                if (excCoup[excPos.location].monA > pscnumber && excCoup[excPos.location].monB > pscnumber)
                {
                  excPos.state = 'c';

                  if (i == 0 && j < 2)
                  {
                    run << "Chargeseparation." << '\n';
                  }

                  sepLocation = excCoup[excPos.h_location_lastS].position;

                  time_ex[i][j] = time;
                  ex_diss[i]++;
                  //excPos.state = 's';//till chargemovement is implemented
                  vel_ex[i][j] = length(excCoup[startPind[i]].position, excCoup[excPos.h_location_lastS].position) / time_ex[i][j];
                }
                viablePartners.clear();//empties vector containing possible partners for step so it can be reused in next step
                partnerConnections.clear();
                break;
              }
              else if (raten.back() <= rate_KMC)
              {
                if (i == 0 && j < 2)
                {
                  run << "Radiating decay." << '\n';
                }

                radiating[i]++;
                excPos.state = 't';
                viablePartners.clear();//empties vector containing possible partners for step so it can be reused in next step
                partnerConnections.clear();
                break;
              }
            }


          }//state e end
          //CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
          else if (excPos.state == 'c')//charge separated state
          {
            if (nscnumber == 0)//if only pSC are availeable
            {
              if (i == 0 && j < 2)
              {
                run << "Only Exciton movement in p-type semiconductor possible, due to lack of n-type semiconductors.";
              }

              excPos.state = 's';
              //break;
            }

            //hole rates in pSC
            for (std::size_t p = 0u; p < h_partnerConnections.size(); p++)
            {
              if (excCoup[h_partnerConnections[p].partnerIndex].monA <= pscnumber && 
                  excCoup[h_partnerConnections[p].partnerIndex].monB <= pscnumber)//movement on pSC
              {
                random_normal = distributionN(engine);//generating normal distributed random number
                coulombenergy = coulomb(excCoup[excPos.h_location].position, excCoup[h_partnerConnections[p].partnerIndex].position, 3.4088) - 
                                coulomb(excCoup[excPos.h_location].position, excCoup[excPos.location].position, 3.4088);

                rate_sum += marcus(h_partnerConnections[p].avgCoup, (random_normal - random_normal1) + coulombenergy, reorganisationsenergie_charge);
              }
              else if (excCoup[h_partnerConnections[p].partnerIndex].monA > pscnumber && 
                       excCoup[h_partnerConnections[p].partnerIndex].monB > pscnumber && 
                       h_partnerConnections[p].partnerIndex == excPos.location)//movement to nSC --> recombination | only possible if electron present on nSC dimer
              {
                random_normal = distributionN(engine);//generating normal distributed random number
                coulombenergy = coulomb(excCoup[excPos.location].position, excCoup[h_partnerConnections[p].partnerIndex].position, 1);
                rate_sum += marcus(h_partnerConnections[p].avgsecCoup, (random_normal - random_normal1) - coulombenergy, reorganisationsenergie_rek);
              }
              else if (excCoup[h_partnerConnections[p].partnerIndex].monA > pscnumber && 
                       excCoup[h_partnerConnections[p].partnerIndex].monB > pscnumber && 
                       h_partnerConnections[p].partnerIndex != excPos.location)
              { //recombination only possible if electron is pressent on nSC dimer => no hopping to nSC if no electron present
                //tmp_ratesum = rate_sum;
                rate_sum += 0.0;
                //heterodimer = true;
              }
              else//to prevent heterodimersfrom participating as electron location HOW TO HANDLE HETERO DIMERS? holediffusion or recombination?
              {
                //tmp_ratesum = rate_sum;
                rate_sum += 0.0;
                //heterodimer = true;
              }

              raten_hole.push_back(rate_sum);

              //if (heterodimer) //set rate_sum back to previous value
              //{
              //  heterodimer = false;
              //  rate_sum = tmp_ratesum;
              //}
              if (i == 0 && j < 2)
              {
                run << "Partner: " << h_partnerConnections[p].partnerIndex << " h_Rates: " << rate_sum << '\n';
              }
            }//p

            //electron rates in nSC
            for (std::size_t p = 0u; p < partnerConnections.size(); p++)
            {
              if (excCoup[partnerConnections[p].partnerIndex].monA > pscnumber &&
                  excCoup[partnerConnections[p].partnerIndex].monB > pscnumber)//movement on nSC
              {
                random_normal = distributionN(engine);//generating normal distributed random number
                coulombenergy = coulomb(excCoup[excPos.location].position, excCoup[partnerConnections[p].partnerIndex].position, 3.4088) - 
                                coulomb(excCoup[excPos.location].position, excCoup[excPos.h_location].position, 3.4088);
                rateFul_sum += marcus(partnerConnections[p].avgCoup, (random_normal - random_normal1) - coulombenergy, reorganisationsenergie_nSC);
              }
              else if (excCoup[partnerConnections[p].partnerIndex].monA < pscnumber && 
                       excCoup[partnerConnections[p].partnerIndex].monB < pscnumber && 
                       partnerConnections[p].partnerIndex == excPos.h_location)//movement to pSC --> recombination | only possible if electron present on nSC dimer
              {
                random_normal = distributionN(engine);//generating normal distributed random number
                coulombenergy = coulomb(excCoup[excPos.h_location].position, excCoup[partnerConnections[p].partnerIndex].position, 1);
                rateFul_sum += marcus(partnerConnections[p].avgsecCoup, (random_normal - random_normal1) + coulombenergy, reorganisationsenergie_rek);
              }
              else if (excCoup[partnerConnections[p].partnerIndex].monA < pscnumber && 
                       excCoup[partnerConnections[p].partnerIndex].monB < pscnumber && 
                       partnerConnections[p].partnerIndex != excPos.h_location)
              { //recombination only possible if hole is pressent on pSC dimer => no hopping to pSC if no hole present
                //tmp_ratesum = rateFul_sum;
                rateFul_sum += 0.0;
                //heterodimer = true;
              }
              else//to prevent heterodimersfrom participating as electron location HOW TO HANDLE HETERO DIMERS? holediffusion or recombination?
              {
                //tmp_ratesum = rateFul_sum;
                rateFul_sum += 0.0;
                //heterodimer = true;
              }

              raten.push_back(rateFul_sum);

              //if (heterodimer) //set rate_sum back to previous value
              //{
              //  heterodimer = false;
              //  rateFul_sum = tmp_ratesum;
              //}

              if (i == 0 && j < 2)
              {
                run << "Partner: " << partnerConnections[p].partnerIndex << " e_Rates: " << rateFul_sum << '\n';
              }
            }

            //decide hopping particle

            if ((1 / rate_sum - time_p) < (1 / rateFul_sum - time_n))
            {
              if (i == 0 && j < 2)
              {
                run << "pSC hopps first. " << std::endl;
              }

              //Update time
              if ((1 / rate_sum - time_p) > 0)
              {
                time += (1 / rate_sum - time_p);
                time_n += (1 / rate_sum - time_p);
                time_p = 0.;
              }
              else if ((1 / rate_sum - time_p) < 0)
              {
                //time = time  //not necessary but intendet to help readeability
                //time_n = time_n
                time_p = 0.;
              }
              else
              {
                throw std::logic_error("Something went wrong with the time. Call the Doctor.");
              }

              //do hopping
              auto random_real = distributionR(engine);
              rate_KMC = random_real * rate_sum;

              for (std::size_t g = 0; g < h_viablePartners.size(); g++)
              {
                if (raten_hole[g] > rate_KMC)
                {
                  if (excCoup[h_partnerConnections[g].partnerIndex].monA <= pscnumber && 
                      excCoup[h_partnerConnections[g].partnerIndex].monB <= pscnumber)
                  {
                    if (i == 0 && j < 2)
                    {
                      run << "Chargetransport" << std::endl;
                    }

                    excPos.h_location_lastS = excPos.h_location;
                    excPos.h_location = h_viablePartners[g];

                    /*End criteria for simulation*/

                    switch (plane)
                    {
                    case 'x':
                      if (averagePandN[0].x() < averagePandN[1].x())//**Determining in which dirrection the hole has to move to move away from the interface
                      {
                        if ((excCoup[excPos.h_location].position.x()/* - avg.x()*/) < (chsepscaling * sepLocation.x()))//(excCoup[startPind[i]].position.x() - avg.x()))
                        {
                          ch_separation[i]++;
                          time_ch[i][j] = time - time_ex[i][j];
                          //vel_ch[i][j] = std::abs(excCoup[excPos.h_location].position.x() - sepLocation.x()) / time_ch[i][j]; //old formula for chargecarrier velocity
                          vel_ch[i][j] = length(sepLocation, excCoup[excPos.h_location].position) / time_ch[i][j];

                          if (i == 0 && j < 2)
                          {
                            run << "Chargeseparation" << std::endl;
                          }

                          excPos.state = 's';
                        }
                      }
                      else if (averagePandN[0].x() > averagePandN[1].x())
                      {
                        if ((excCoup[excPos.h_location].position.x()/* - avg.x()*/) > ((2 - chsepscaling) * sepLocation.x()))//(excCoup[startPind[i]].position.x() - avg.x()))
                        {
                          ch_separation[i]++;
                          time_ch[i][j] = time - time_ex[i][j];
                          vel_ch[i][j] = length(sepLocation, excCoup[excPos.h_location].position) / time_ch[i][j];

                          if (i == 0 && j < 2)
                          {
                            run << "Chargeseparation" << std::endl;
                          }

                          excPos.state = 's';
                        }
                      }
                      break;

                    case 'y':
                      if (averagePandN[0].y() < averagePandN[1].y())//**Determining in which dirrection the hole has to move to move away from the interface
                      {
                        if ((excCoup[excPos.h_location].position.y() /*- avg.y()*/) < (chsepscaling * sepLocation.y()))/*(excCoup[startPind[i]].position.y() - avg.y()))*/
                        {
                          ch_separation[i]++;
                          time_ch[i][j] = time - time_ex[i][j];
                          vel_ch[i][j] = length(sepLocation, excCoup[excPos.h_location].position) / time_ch[i][j];

                          if (i == 0 && j < 2)
                          {
                            run << "Chargeseparation" << std::endl;
                          }

                          excPos.state = 's';
                        }
                      }
                      else if (averagePandN[0].y() > averagePandN[1].y())
                      {
                        if ((excCoup[excPos.h_location].position.y() /*- avg.y()*/) > ((2 - chsepscaling) * sepLocation.y()))/*(excCoup[startPind[i]].position.y() - avg.y()))*/
                        {
                          ch_separation[i]++;
                          time_ch[i][j] = time - time_ex[i][j];
                          vel_ch[i][j] = length(sepLocation, excCoup[excPos.h_location].position) / time_ch[i][j];

                          if (i == 0 && j < 2)
                          {
                            run << "Chargeseparation" << std::endl;
                          }

                          excPos.state = 's';
                        }
                      }
                      break;

                    case 'z':
                      if (averagePandN[0].z() < averagePandN[1].z())//**Determining in which dirrection the hole has to move to move away from the interface
                      {
                        if ((excCoup[excPos.h_location].position.z() /*- avg.z()*/) < (chsepscaling * sepLocation.z()))/*(excCoup[startPind[i]].position.z() - avg.z())*/
                        {
                          ch_separation[i]++;
                          time_ch[i][j] = time - time_ex[i][j];
                          vel_ch[i][j] = length(sepLocation, excCoup[excPos.h_location].position) / time_ch[i][j];

                          if (i == 0 && j < 2)
                          {
                            run << "Chargeseparation" << std::endl;
                          }
                          excPos.state = 's';
                        }
                      }
                      else if (averagePandN[0].z() > averagePandN[1].z())
                      {
                        if ((excCoup[excPos.h_location].position.z() /*- avg.z()*/) > ((2 - chsepscaling) * sepLocation.z()))/*(excCoup[startPind[i]].position.z() - avg.z())*/
                        {
                          ch_separation[i]++;
                          time_ch[i][j] = time - time_ex[i][j];
                          vel_ch[i][j] = length(sepLocation, excCoup[excPos.h_location].position) / time_ch[i][j];

                          if (i == 0 && j < 2)
                          {
                            run << "Chargeseparation" << std::endl;
                          }
                          excPos.state = 's';
                        }
                      }
                      break;
                    }//switch end

                    //#########################################################################################################################
                    if (i == 0 && j < 2)
                    {
                      run << "old pSC Monomer " << std::setw(5) << excPos.h_location_lastS << std::endl;
                      run << "new pSC Monomer " << std::setw(5) << excPos.h_location << std::endl;
                      run << "Coupling pSC " << std::setw(5) << std::setprecision(6) << std::fixed << avgCoup << std::endl;
                      run << "nSC " << std::setw(5) << excPos.location << std::setw(5) << excPos.location_lastS << std::endl;
                    }
                    break;

                  }
                  else if (excCoup[partnerConnections[g].partnerIndex].monA > pscnumber && 
                           excCoup[partnerConnections[g].partnerIndex].monB > pscnumber)//movement of electron back onto pSC
                  {
                    if (i == 0 && j < 2)
                    {
                      run << "Recombination" << std::endl;
                    }

                    excPos.state = 't';
                    rekombined[i]++;
                    break;
                  }
                }
                else if (g == h_viablePartners.size())
                {
                  throw std::logic_error("WARNING: Errot during pSC chargetransfer, no suiteable hopping target was found.");
                  return;
                }
              }

            }//end of pSC hopping

            //nsc hopping
            else if ((1 / rate_sum - time_p) > (1 / rateFul_sum - time_n))
            {
              if (i == 0 && j < 2)
              {
                run << "Hopping on nSC first." << std::endl;
              }

              if ((1 / rateFul_sum - time_n) > 0)
              {
                time = time + (1 / rateFul_sum - time_n);
                time_p = time_p + ((1 / rateFul_sum - time_n));
                time_n = 0;
              }
              else if ((1 / rateFul_sum - time_n) < 0)
              {
                //time = time;
                //time_p = time_p;
                time_n = 0;
              }
              else
              {
                throw std::logic_error("Something went wrong with the time. Call the Doctor.");
              }

              //execute nSC hopping
              auto random_real = distributionR(engine);
              rate_KMC = random_real * rateFul_sum;

              for (std::size_t g = 0; g < viablePartners.size(); g++)
              {
                if ((raten[g] > rate_KMC))
                {
                  if (excCoup[partnerConnections[g].partnerIndex].monA > pscnumber &&
                      excCoup[partnerConnections[g].partnerIndex].monB > pscnumber)
                  {
                    if (i == 0 && j < 2)
                    {
                      run << "Chargetransport in nSC" << std::endl;
                    }

                    excPos.location_lastS = excPos.location;
                    excPos.location = viablePartners[g];

                    //#########################################################################################################################
                    if (i == 0 && j < 2)
                    {
                      run << "old nSC Monomer " << std::setw(5) << excPos.location_lastS << std::endl;
                      run << "new nSC Monomer " << std::setw(5) << excPos.location << std::endl;
                      run << "Coupling nSC " << std::setw(5) << std::setprecision(6) << std::fixed << avgCoup << std::endl;
                      run << "pSC " << std::setw(5) << excPos.h_location << std::setw(5) << excPos.h_location_lastS << std::endl;
                    }

                    break;
                  }
                  else if (excCoup[partnerConnections[g].partnerIndex].monA <= pscnumber && 
                           excCoup[partnerConnections[g].partnerIndex].monB <= pscnumber)
                  {
                    if (i == 0 && j < 2)
                    {
                      run << "Recombination" << std::endl;
                    }

                    excPos.state = 't';
                    rekombined[i]++;
                    break;
                  }
                }
                else if (g == viablePartners.size())
                {
                  throw std::logic_error("WARNING: ERROR during transport in nSC");
                  return;
                }
              }



            }//end nSC hopping

            viablePartners.clear();//empties vector containing possible partners for step so it can be reused in next step
            h_viablePartners.clear();
            partnerConnections.clear();
            h_partnerConnections.clear();
            //break;
          }// state c end
          //SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS
          else if (excPos.state == 's')//separated state
          {
            if (i == 0 && j < 2)
            {
              run << "Successful run." << '\n';
            }

            viablePartners.clear();//empties vector containing possible partners for step so it can be reused in next step
            partnerConnections.clear();
            h_viablePartners.clear();
            h_partnerConnections.clear();
            check_maxSteps = false;
            excPos.state = 'e';
            break;
          }//state s end
          //TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT
          else if (excPos.state == 't')//termination state
          {
            if (i == 0 && j < 2)
            {
              run << "Broken.-" << '\n';
            }

            viablePartners.clear();//empties vector containing possible partners for step so it can be reused in next step
            partnerConnections.clear();
            h_viablePartners.clear();
            h_partnerConnections.clear();
            check_maxSteps = false;
            excPos.state = 'e';
            break;
          }// state t end
          //??????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
          else
          {
            throw std::logic_error("Something somewhere went terribly wrong and the simulation ended up in an unknown state.");
          }

          viablePartners.clear();//empties vector containing possible partners for step so it can be reused in next step
          partnerConnections.clear();
          h_viablePartners.clear();
          h_partnerConnections.clear();

          if (i == 0 && j < 2)
          {
            run << "Exciton Position after Movement:  " << excPos.location << " Monomer A: " << excCoup[excPos.location].monA
              << " Monomer B: " << excCoup[excPos.location].monB << '\n';
            run << "Step: " << h << '\n';
            run << "#################################################################################" << '\n';
          }
        }//loop for steps h

        if (check_maxSteps)
        {
          if (i == 0 && j < 2)
          {
            run << "No exciton or charge Separation, particels ran maximum number of steps." << '\n';
          }

          if (excPos.state == 'e')
          {
            if (i == 0 && j < 2)
            {
              run << "Exciton ran into oblivion." << '\n';
            }
          }
          else
          {
            if (i == 0 && j < 2)
            {
              run << "Chargecarriers ran into oblivion." << '\n';
            }
          }
          excPos.state = 'e';
        }

        if (i == 0 && j < 2)
        {
          run << "Try: " << j << '\n';
          run << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%" << '\n';
        }
      }//100 try loop j
    }//loop over startingpoints i

    //___________________________________EVALUATION_______________________________________________________________________________________

    std::ofstream evaluation;
    evaluation.open("evaluation.txt");
    evaluation << std::setw(4) << "k" << std::setw(4) << "IX" << std::setw(9) << "Ex_Diss" << std::setw(9) << "Ch_Diss" 
               << std::setw(9) << "Rek." << std::setw(9) << "Trapp." << std::setw(9) << "Fluor." << std::endl;

    double avg_ex(0.), avg_ch(0.), avg_rek(0.), avg_trap(0.), avg_rad(0.);

    for (std::size_t k = 0; k < startPind.size(); k++)
    {
      avg_ex += ex_diss[k];
      avg_ch += ch_separation[k];
      avg_rek += rekombined[k];
      avg_trap += trapped[k];
      avg_rad += radiating[k];

      evaluation << std::setw(4) << k << std::setw(4) << startPind[k] << std::setw(9) << std::setprecision(5) << ex_diss[k]
        << std::setw(9) << std::setprecision(5) << ch_separation[k] << std::setw(9) << std::setprecision(5) << rekombined[k]
        << std::setw(9) << std::setprecision(5) << trapped[k] << std::setw(9) << std::setprecision(5) << radiating[k] << std::endl;
    }

    evaluation << std::setw(9) << "Average " << std::setw(9) << std::setprecision(5) << std::fixed << avg_ex / startPind.size()
      << std::setw(9) << std::setprecision(5) << std::fixed << avg_ch / startPind.size()
      << std::setw(9) << std::setprecision(5) << std::fixed << avg_rek / startPind.size()
      << std::setw(9) << std::setprecision(5) << std::fixed << avg_trap / startPind.size()
      << std::setw(9) << std::setprecision(5) << std::fixed << avg_rad / startPind.size() << std::endl;

    evaluation << "Velocities " << std::endl;
    evaluation << std::setw(4) << "k" << std::setw(5) << "IX" << std::setw(11) << "Ex_vel" << " " << std::setw(11) << "Ex_s_dev" << " " << std::setw(11)
               << "Ch_vel" << " " << std::setw(11) << "Ch_s_dev" << std::endl;

    //average veloceties
    std::vector <double> avg_ex_vel, avg_ch_vel;
    std::vector <double> standDevEX, standDevCH;

    for (std::size_t k = 0; k < startPind.size(); k++)
    {
      double tmp_avg_ch_vel(0.);
      double tmp_avg_ex_vel(0.);
      for (std::size_t j = 0; j < nbrof_tries; j++)// sum up all veloceties for exciton and holes
      {

        if (vel_ch[k][j] > 0.0001)
        {
          tmp_avg_ch_vel += vel_ch[k][j];
        }

        if (vel_ex[k][j] > 0.0001)
        {
          tmp_avg_ex_vel += vel_ex[k][j];
        }
      }//j
      avg_ch_vel.push_back(tmp_avg_ch_vel);
      avg_ex_vel.push_back(tmp_avg_ex_vel);
      // divide summed up velocities by number of relevant events happened
      if (ch_separation[k] > 0)
      {
        avg_ch_vel[k] /= ch_separation[k];
      }
      else if (ch_separation[k] == 0)
      {
        avg_ch_vel[k] = 0;
      }

      if (ex_diss[k] > 0)
      {
        avg_ex_vel[k] /= ex_diss[k];
      }
      else if (ex_diss[k] == 0)
      {
        avg_ex_vel[k] = 0;
      }

      //calculation of standart deviation
      double tmpstandDevEX(0.), tmpstandDevCH(0.);

      for (std::size_t j = 0; j < nbrof_tries; j++)
      {
        if (vel_ch[k][j] > 0.0001)
        {
          tmpstandDevCH += ((vel_ch[k][j] - avg_ch_vel[k]) * (vel_ch[k][j] - avg_ch_vel[k]));
        }

        if (vel_ex[k][j] > 0.0001)
        {
          tmpstandDevEX += ((vel_ex[k][j] - avg_ex_vel[k]) * (vel_ex[k][j] - avg_ex_vel[k]));
        }
      }//j
      standDevCH.push_back(tmpstandDevCH);
      standDevEX.push_back(tmpstandDevEX);

      if (ch_separation[k] > 1)
      {
        standDevCH[k] = sqrt(standDevCH[k] / ch_separation[k]);
      }
      else if (ch_separation[k] < 2)
      {
        standDevCH[k] = 0;
      }

      if (ex_diss[k] > 1)
      {
        standDevEX[k] = sqrt(standDevEX[k] / ex_diss[k]);
      }
      else if (ex_diss[k] < 2)
      {
        standDevEX[k] = 0;
      }

      evaluation << std::setw(4) << k << std::setw(5) << startPind[k] << std::setw(11) << std::setprecision(5) << std::fixed << avg_ex_vel[k] * 1e-9 <<
        " " << std::setw(11) << std::setprecision(5) << std::fixed << standDevEX[k] * 1e-9 <<
        " " << std::setw(11) << std::setprecision(5) << std::fixed << avg_ch_vel[k] * 1e-9 <<
        " " << std::setw(11) << std::setprecision(5) << std::fixed << standDevCH[k] * 1e-9 << std::endl;
    }//k

    double mean_vel_ex(0.), mean_vel_ch(0.);
    for (std::size_t k = 0; k < startPind.size(); k++)
    {
      mean_vel_ex += avg_ex_vel[k];
      mean_vel_ch += avg_ch_vel[k];
    }

    mean_vel_ex /= startPind.size();
    mean_vel_ch /= startPind.size();

    evaluation << std::left << std::setw(7) << "Average    " << std::left << std::setw(22) << std::setprecision(5) << std::fixed << mean_vel_ex * 1e-9 <<
      " " << std::left << std::setw(9) << std::setprecision(5) << std::fixed << mean_vel_ch * 1e-9 << std::endl;

    //distribution of chargecarrier and exciton-velocities
    std::ofstream distribution;
    distribution.open("Exciton_Distribution.txt");
    int nmbr;

    for (std::size_t i = 0; i < 20; i++)
    {
      nmbr = 0;

      for (std::size_t k = 0; k < startPind.size(); k++)
      {
        for (std::size_t j = 0; j < nbrof_tries; j++)
        {
          if ((vel_ex[k][j] > ((i + 1) * 50 * 1e9)))
          {
            nmbr++;
          }
        }
      }
      distribution << std::setw(9) << std::setprecision(5) << (i + 1) * 50 << std::setw(9) << nmbr / startPind.size() << std::endl;
    }
    distribution.close();

    distribution.open("Charge_Distribution.txt");

    for (std::size_t i = 0; i < 20; i++)
    {
      nmbr = 0;

      for (std::size_t k = 0; k < startPind.size(); k++)
      {
        for (std::size_t j = 0; j < nbrof_tries; j++)
        {
          if (vel_ch[k][j] > ((i + 1) * 50 * 1e9))
          {
            nmbr++;
          }
        }
      }
      distribution << std::setw(9) << std::setprecision(5) << (i + 1) * 50 << std::setw(9) << nmbr / (startPind.size()) << std::endl;
    }

    distribution.close();

    run.close();

  }//try
  catch (std::exception& e)
  {
    std::cout << "An exception occured. The execution of " << config::Programname << " failed. \n";
    std::cout << "Error: " << e.what() << '\n';
  }
}
