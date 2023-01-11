
//////////   //////////   //////////   ////////// 
//           //      //   //               //
//           //      //   //               //
//           //      //   //////////       //
//           //////////           //       //
//           //      //           //       //
//           //      //           //       //
//////////   //      //   //////////       //
/////conformational analysis and search tool/////

// If we run our tests, we will
// take the testing main from gtest/testing_main.cc
#ifndef GOOGLE_MOCK

//////////////////////////
//                      //
//       L I B S        //
//                      //
//////////////////////////
#ifdef USE_PYTHON
#include <Python.h>
#endif
#include <cstdlib>
#include <fstream>
#include <memory>
#include <omp.h>

//////////////////////////
//                      //
//    H E A D E R S     //
//                      //
//////////////////////////
#include "configuration.h"
#include "coords_io.h"
#include "Scon/scon_chrono.h"
#include "helperfunctions.h"
#include "Scon/scon_log.h"
#ifdef _MSC_VER
#include "win_inc.h"
#endif
// Task items
#include "startopt_solvadd.h"
#include "startopt_ringsearch.h"
#include "md.h"
#include "optimization_global.h"
#include "pathopt.h"
#include "alignment.h"
#include "entropy.h"
#include "Path_perp.h"
//#include "Matrix_Class.h" //For ALIGN, PCAgen, ENTROPY, PCAproc
#include "TrajectoryMatrixClass.h" //For ALIGN, PCAgen, ENTROPY, PCAproc
#include "PCA.h"
#include "2DScan.h"
#include "exciton_breakup.h"
#include "Center.h"
#include "Couplings.h"
#include "periodicCutout.h"
#include "replaceMonomers.h"
#include "modify_sk.h"
#include "excitonDiffusion.h"
#include "ic_exec.h"
#include "optimization.h"
#include "find_as.h"
#include "pmf_ic_prep.h"

//////////////////////////
//                      //
//  R A N D O M         //
//  N U M B E R         //
//  G E N E R A T O R   //
//                      //
//////////////////////////
#if defined(_MSC_VER)
#include <process.h>
#define pid_func _getpid
#else 
#include <unistd.h>
#define pid_func getpid
#endif


// Enable this define to drop exceptions
//
// If CAST_DEBUG_DROP_EXCEPTIONS is set,
// exceptions will be dropped (this is good for debugging
// and default in Debug configuration.
// Otherwise CAST will crash and print the string
// attached to the exception. This is default behaviour 
// in release mode.
//
//#define CAST_DEBUG_DROP_EXCEPTIONS

int main(int argc, char** argv)
{
#ifdef USE_PYTHON   
  Py_Initialize();  // if python enabled: initialise here
#endif

#ifndef CAST_DEBUG_DROP_EXCEPTIONS
  try
  {
#endif

    //////////////////////////
    //                      //
    //      Preparation     //
    //                      //
    //////////////////////////

    std::cout << scon::c3_delimeter(',');

    // start execution and initialization timer
    scon::chrono::high_resolution_timer exec_timer, init_timer;

    // initialize (old) Random Number Generator
    srand((unsigned int)time(NULL) + pid_func());
    
    // Parse config file and command line 
    auto config_filename = config::config_file_from_commandline(argc, argv);
    Config main_configuration(config_filename);
    config::parse_command_switches(argc, argv);

    // Print configuration
    if (Config::get().general.verbosity > 1U)
    {
      std::cout << "\n";
      std::cout << "  |-----------------------------------------------------|\n";
      std::cout << "  |                                                     |\n";
      std::cout << "  |  //////////   //////////   //////////   //////////  |\n";
      std::cout << "  |  //           //      //   //               //      |\n";
      std::cout << "  |  //           //      //   //               //      |\n";
      std::cout << "  |  //           //      //   //////////       //      |\n";
      std::cout << "  |  //           //////////           //       //      |\n";
      std::cout << "  |  //           //      //           //       //      |\n";
      std::cout << "  |  //           //      //           //       //      |\n";
      std::cout << "  |  //////////   //      //   //////////       //      |\n";
      std::cout << "  |                                                     |\n";
      std::cout << "  |       conformational analysis and search tool       |\n";
      std::cout << "  |                                                     |\n";
      std::cout << "  |-----------------------------------------------------|\n\n\n";

      std::cout << "-------------------------------------------------------\n";
      std::cout << "Configuration ('" << config_filename << "')\n";
      std::cout << "-------------------------------------------------------\n";
      std::cout << Config::get().general;
      std::cout << Config::get().coords;
      std::cout << Config::get().energy;
      std::cout << Config::get().periodics;
    }

    //////////////////////////
    //                      //
    //    Initialization    //
    //                      //
    //////////////////////////

    if (Config::get().general.energy_interface == config::interface_types::T::DFTBABY)
    {
#ifdef USE_PYTHON
#else
      throw std::runtime_error("It is not possible to use DFTBaby without python!");
#endif
      std::remove("output_dftb.txt"); // delete dftbaby output files from former run
      std::remove("tmp_struc_trace.xyz");
    }

    // read coordinate input file
    // "ci" contains all the input structures
    std::unique_ptr<coords::input::format> ci(coords::input::new_format());
    coords::Coordinates coords(ci->read(Config::get().general.inputFilename));

    // Print "Header"
    if (Config::get().general.verbosity > 1U)
    {
      std::cout << "-------------------------------------------------\n";
      std::cout << "Initialization\n";
      std::cout << "-------------------------------------------------\n";
      std::cout << "Loaded " << ci->size() << " structure" << (ci->size() == 1 ? "" : "s");
      std::cout << ". (" << ci->atoms() << " atom" << (ci->atoms() == 1 ? "" : "s");
      std::cout << " and " << coords.weight() << " g/mol";
      std::cout << (ci->size() > 1 ? " each" : "") << ")\n";
      std::size_t const susysize(coords.subsystems().size());
      if (susysize > 1U)
      {
        std::cout << susysize << " subsystems: ";
        for (std::size_t i(0U); i < susysize; ++i)
        {
          std::size_t const atms(coords.subsystems(i).size());
          std::cout << "[#" << i + 1 << " with " << atms << " atom" << (atms == 1 ? ".]" : "s.]");
        }
        std::cout << '\n';
      }
    }

    // If Periodic Boundry Conditions are used, translate all structures
    // so that their center of mass is on the origin of the coordinate system
    if (Config::get().periodics.periodic)
    {
      for (auto& pes : *ci)
      {
        coords.set_xyz(pes.structure.cartesian);
        coords.move_all_by(-coords.center_of_mass());
        pes = coords.pes();
      }
      // If Cutout option is on, cut off all atoms outside of box + radius
      if (Config::get().periodics.periodicCutout)
      {
        coords::Coordinates newCoords(coords);
        for (auto& pes : *ci)
        {
          newCoords.set_xyz(pes.structure.cartesian);
          newCoords = periodicsHelperfunctions::periodicCutout(coords);
          pes = newCoords.pes();
        }
        newCoords.set_xyz(ci->structure(0u).structure.cartesian);
        coords = newCoords;
      }
    }



    // stop and print initialization time
    if (Config::get().general.verbosity > 1U)
    {
      std::cout << "-------------------------------------------------\n";
      std::cout << "Initialization done after " << init_timer << '\n';
    }

    //////////////////////////
    //                      //
    //    Preoptimization   //
    //                      //
    //////////////////////////

    if (coords.preoptimize())
    {
      if (Config::get().general.verbosity > 1U)
      {
        std::cout << "-------------------------------------------------\n";
        std::cout << "Preoptimization:\n";
        std::cout << "-------------------------------------------------\n";
      }
      std::size_t i(0);
      for (auto& pes : *ci)
      {
        coords.set_xyz(pes.structure.cartesian);
        coords.pe();
        if (Config::get().general.verbosity > 1U)
        {
          std::cout << "Preoptimization initial: " << ++i << '\n';
          coords.e_head_tostream_short(std::cout, coords.preinterface());
          coords.e_tostream_short(std::cout, coords.preinterface());
        }
        coords.po();
        pes.structure.cartesian = coords.xyz();
        if (Config::get().general.verbosity > 1U)
        {
          std::cout << "Preoptimization post-opt: " << i << '\n';
          coords.e_tostream_short(std::cout, coords.preinterface());
        }
      }
    }


    //////////////////////////
    //                      //
    //       T A S K S      //
    //                      //
    //////////////////////////

    // print which task
    std::cout << "-------------------------------------------------\n";
    std::cout << "Task '" << config::task_strings[Config::get().general.task];
    std::cout << "' (" << Config::get().general.task << ") computation:\n";
    std::cout << "-------------------------------------------------\n";

    // start task timer
    scon::chrono::high_resolution_timer task_timer;

    // select task
    switch (Config::get().general.task)
    {
    case config::tasks::DEVTEST:
    {
      // DEVTEST: Room for Development Testing
            /**
       * THIS TASK PERFORMS PRINCIPAL COMPONENT ANALYSIS ON A SIMULATION TRAJECTORY
       *
       * This task will perform a principal component analysis (PCA) on a molecular simulation
       * trajectory. Prior translational- and rotational fit of the conformations
       * obtained is possible. Options can be specified in the INPUTFILE.
       *
       * Further processing can be done via PCAproc - Task
       */

       // Create empty pointer since we do not know yet if PCA eigenvectors etc.
       // will be generated from coordinates or read from file
      pca::PrincipalComponentRepresentation* pcaptr = nullptr;

      if (Config::get().PCA.two_traj)   // compare two trajectories
      {
        // get eigenvectors of combined trajectory
        coords::Coordinates coords(ci->read(Config::get().PCA.second_traj_name));
        pcaptr = new pca::PrincipalComponentRepresentation();
        pcaptr->generateCoordinateMatrix(ci, coords);
        pcaptr->generatePCAEigenvectorsFromCoordinates();

        // apply eigenvectors on first trajectory and write out histogram
        std::unique_ptr<coords::input::format> ci1(coords::input::new_format());
        coords::Coordinates coords1(ci1->read(Config::get().general.inputFilename));
        pcaptr->generateCoordinateMatrix(ci1, coords1);
        pcaptr->generatePCAModesFromPCAEigenvectorsAndCoordinates();
        pcaptr->writePCAModesFile("pca_modes_1.dat");
        pcaptr->writeHistogrammedProbabilityDensity("pca_histogrammed_1.dat");

        // apply eigenvectors on second trajectory and write out histogram
        std::unique_ptr<coords::input::format> ci2(coords::input::new_format());
        coords::Coordinates coords2(ci2->read(Config::get().PCA.second_traj_name));
        pcaptr->generateCoordinateMatrix(ci2, coords2);
        pcaptr->generatePCAModesFromPCAEigenvectorsAndCoordinates();
        pcaptr->writePCAModesFile("pca_modes_2.dat");
        pcaptr->writeHistogrammedProbabilityDensity("pca_histogrammed_2.dat");
      }
      else    // "normal" PCA
      {
        // Create new PCA eigenvectors and modes
        if (!Config::get().PCA.pca_read_modes && !Config::get().PCA.pca_read_vectors)
        {
          pcaptr = new pca::PrincipalComponentRepresentation(ci, coords);
          pcaptr->writePCAModesFile("pca_modes.dat");
          pcaptr->writePCAModesBinaryFile("pca_modes.cbf");
        }
        // Read modes and eigenvectors from (properly formated) file "pca_modes.dat"
        else if (Config::get().PCA.pca_read_modes && Config::get().PCA.pca_read_vectors)
        {
          pcaptr = new pca::PrincipalComponentRepresentation("pca_modes.dat");
          pcaptr->generateCoordinateMatrix(ci, coords);  // this is necessary in case of truncated coordinates
        }
        else
        {
          pcaptr = new pca::PrincipalComponentRepresentation();
          // Read PCA-Modes from file but generate new eigenvectors from input coordinates (I think this doesn't make much sense???)
          if (Config::get().PCA.pca_read_modes)
          {
            pcaptr->generateCoordinateMatrix(ci, coords);
            pcaptr->generatePCAEigenvectorsFromCoordinates();
            pcaptr->readModes("pca_modes.dat");
          }
          // Read PCA-Eigenvectors from file but generate new modes using the eigenvectors
          // and the input coordinates
          else if (Config::get().PCA.pca_read_vectors)
          {
            pcaptr->generateCoordinateMatrix(ci, coords);
            pcaptr->readBinary("pca_modes.cbf",true,true,false,false);
            pcaptr->generatePCAModesFromPCAEigenvectorsAndCoordinates();
          }
        }

        // If modes or vectors have changed, write them to new file
        if (Config::get().PCA.pca_read_modes != Config::get().PCA.pca_read_vectors)
        {
          pcaptr->writePCAModesFile("pca_modes_new.dat");
          pcaptr->writePCAModesBinaryFile("pca_modes_new.cbf");
        }

        // Create Histograms
        // ATTENTION: This function read from Config::PCA
        pcaptr->writeHistogrammedProbabilityDensity("pca_histogrammed.dat");

        // Write Stock's Delta, see DOI 10.1063/1.2746330
        // ATTENTION: This function read from Config::PCA
        pcaptr->writeStocksDelta("pca_stocksdelta.dat");

      }
      std::cout << "";
      //
      const kNN_NORM norm = static_cast<kNN_NORM>(Config::get().entropy.knnnorm);
      const kNN_FUNCTION func = static_cast<kNN_FUNCTION>(Config::get().entropy.knnfunc);
      //
      
      auto PCAobj = entropyobj(pcaptr->getModes().t(), pcaptr->getModes().rows(),pcaptr->getModes().cols());
      auto RAWobj = entropyobj(pcaptr->getMWTrajectoryMatrix().t(), pcaptr->getMWTrajectoryMatrix().rows(), pcaptr->getMWTrajectoryMatrix().cols());
      delete pcaptr;
      pcaptr = new pca::PrincipalComponentRepresentation("pca_modes.cbf");
      auto PCAREREADBINobj = entropyobj(pcaptr->getModes().t(), pcaptr->getModes().rows(), pcaptr->getModes().cols());
      delete pcaptr;
      pcaptr = new pca::PrincipalComponentRepresentation("pca_modes.dat");
      auto PCAREREADTXTobj = entropyobj(pcaptr->getModes().t(), pcaptr->getModes().rows(), pcaptr->getModes().cols());
      //
      auto PCAobj2 = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, PCAobj);
      auto PCAREREADBINobj2 = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, PCAREREADBINobj);
      auto PCAREREADTXTobj2 = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, PCAREREADTXTobj);
      auto RAWobj2 = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, RAWobj);
      //
      double valuePCA = 0.;
      auto PCAkNN = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, PCAobj2);
      valuePCA = PCAkNN.calculateFulldimensionalNNEntropyOfDraws(norm, false);
      std::cout << std::fixed;
      std::cout << std::setprecision(6u);
      std::cout << "Entropy value PCA: " << valuePCA * constants::boltzmann_constant_kb_gaussian_units * constants::eV2kcal_mol * 1000.0 << " cal/(mol*K)\n " << std::endl;
      //
      double valuePCAREREADBIN = 0.;
      auto PCAREREADBINkNN = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, PCAREREADBINobj2);
      valuePCAREREADBIN = PCAREREADBINkNN.calculateFulldimensionalNNEntropyOfDraws(norm, false);
      std::cout << std::fixed;
      std::cout << std::setprecision(6u);
      std::cout << "Entropy value PCAREADBIN: " << valuePCAREREADBIN * constants::boltzmann_constant_kb_gaussian_units * constants::eV2kcal_mol * 1000.0 << " cal/(mol*K)\n " << std::endl;
      //
      double valuePCAREREADTXT = 0.;
      auto PCAREREADTXTkNN = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, PCAREREADTXTobj2);
      valuePCAREREADTXT = PCAREREADTXTkNN.calculateFulldimensionalNNEntropyOfDraws(norm, false);
      std::cout << std::fixed;
      std::cout << std::setprecision(6u);
      std::cout << "Entropy value PCAREADTXT: " << valuePCAREREADTXT * constants::boltzmann_constant_kb_gaussian_units * constants::eV2kcal_mol * 1000.0 << " cal/(mol*K)\n " << std::endl;
      //
      double valueRAW = 0.;
      auto RAWkNN = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, RAWobj2);
      valueRAW = RAWkNN.calculateFulldimensionalNNEntropyOfDraws(norm, false);
      std::cout << std::fixed;
      std::cout << std::setprecision(6u);
      std::cout << "Entropy value RAW: " << valueRAW * constants::boltzmann_constant_kb_gaussian_units * constants::eV2kcal_mol * 1000.0 << " cal/(mol*K)\n " << std::endl;


      // For i in dim:
      // Purge correctly in correct order of modes 
      //// jede mode ist eine row, 1. row ist wichtigste mode
      delete pcaptr;
      pcaptr = new pca::PrincipalComponentRepresentation("pca_modes.cbf");
      std::cout << "------\n" << "Number of Modes: " << pcaptr->getModes().rows() << "\n";
      for (std::size_t i = 0u; i < pcaptr->getModes().rows(); ++i)
      {
        std::cout << "Current dimensionality: " << i+1 << "\n";
        Matrix_Class purgedModesMatrix = pcaptr->getModes();
        if (i+1 <= pcaptr->getModes().rows() - 1u)
        {
          purgedModesMatrix.shed_rows(i+1, pcaptr->getModes().rows()-1u);
        }
        double valuePCATRUNC = 0.;
        auto PCATRUNCkNN = calculatedentropyobj(
          Config::get().entropy.entropy_method_knn_k, entropyobj(
            purgedModesMatrix.t(), purgedModesMatrix.rows(), purgedModesMatrix.cols()
            )
          );
        valuePCATRUNC = PCATRUNCkNN.calculateFulldimensionalNNEntropyOfDraws(norm, false);
        std::cout << std::fixed;
        std::cout << std::setprecision(6u);
        std::cout << "Entropy value PCATRUNC: " << valuePCATRUNC * constants::boltzmann_constant_kb_gaussian_units * constants::eV2kcal_mol * 1000.0 << " cal/(mol*K)\n " << std::endl;
        //
        if (i + 1u == pcaptr->getModes().rows())
        {
          const double valuePCATRUNC_1MIE = PCATRUNCkNN.calculateNN_MIExpansion(1u, norm, func, false);
          std::cout << "Entropy value PCATRUNC_1MIE: " << valuePCATRUNC_1MIE * constants::boltzmann_constant_kb_gaussian_units * constants::eV2kcal_mol * 1000.0 << " cal/(mol*K)\n " << std::endl;
          //
        }

        
        std::cout << "--------- Starting Pruning / Stride -----------\n";
        std::vector<std::size_t> offsetValues = std::vector<std::size_t>{5,10,20};

        for (std::size_t j = 0u; j < offsetValues.size(); ++j)
        {
          const std::size_t o = offsetValues.at(j);
          std::cout << "######\n##o=" << o << "\n";
          //Matrix_Class purgedAndPrunedModesMatrix = purgedModesMatrix;
          //std::size_t numAlreadyPruned = 0u;
          //std::size_t iterAccess =0u;
          //while (iterAccess < purgedAndPrunedModesMatrix.cols())
          //{
          //  const std::size_t startThisPrune = iterAccess + 1u;
          //  const std::size_t stopThisPrune = iterAccess + o;
          //  std::cout << "startThisPrune: " << startThisPrune << " stopThisPrune:" << stopThisPrune << "\n";
          //  std::cout << "numAlreadyPruned: " << numAlreadyPruned;
          //  std::cout << " Matrix-Size: " << purgedAndPrunedModesMatrix.cols() << std::endl;
          //  if (stopThisPrune != startThisPrune)
          //    purgedAndPrunedModesMatrix.shed_cols(startThisPrune,std::min(stopThisPrune, purgedAndPrunedModesMatrix.cols()-1u)); // This doesnt work
          //  iterAccess += 1u;
          //  numAlreadyPruned = numAlreadyPruned + 1u;
          //}
          const std::size_t numFramesAfterPurge = std::size_t(purgedModesMatrix.cols() / double(o));
          Matrix_Class purgedAndPrunedModesMatrix = Matrix_Class(purgedModesMatrix.rows(), numFramesAfterPurge,-1.);
          for (std::size_t k = 0u; k < numFramesAfterPurge; ++k)
          {
            for (std::size_t l = 0u; l < purgedModesMatrix.rows(); ++l)
            {
              purgedAndPrunedModesMatrix(l,k) = purgedModesMatrix(l,k*o);
            }
          }
          std::cout << "---\nSanity Check:\nOriginal Frames: " << purgedModesMatrix.cols() << "\nPruned Frames: " << purgedAndPrunedModesMatrix.cols();
          std::cout << "\no: " << o << std::endl;
          auto PCATRUNCPRUNEDkNN = calculatedentropyobj(
            Config::get().entropy.entropy_method_knn_k, entropyobj(
              purgedAndPrunedModesMatrix.t(), purgedAndPrunedModesMatrix.rows(), purgedAndPrunedModesMatrix.cols()
            )
          );
          const double valuePCATRUNCPRUNED = PCATRUNCPRUNEDkNN.calculateFulldimensionalNNEntropyOfDraws(norm, false);
          std::cout << std::fixed;
          std::cout << std::setprecision(6u);
          std::cout << "Entropy value PCATRUNC: " << valuePCATRUNCPRUNED * constants::boltzmann_constant_kb_gaussian_units * constants::eV2kcal_mol * 1000.0 << " cal/(mol*K)\n " << std::endl;
          if (i + 1u == pcaptr->getModes().rows())
          {
            const double valuePCATRUNCPRUNED_1MIE = PCATRUNCPRUNEDkNN.calculateNN_MIExpansion(1u, norm, func, false);
            std::cout << "Entropy value PCATRUNC_1MIE: " << valuePCATRUNCPRUNED_1MIE * constants::boltzmann_constant_kb_gaussian_units * constants::eV2kcal_mol * 1000.0 << " cal/(mol*K)\n " << std::endl;
          }
          std::cout << "\n";
        }
        
        //

      }
      // Same for kNN
      // Calculate fulldim entropy
      // Calculate 1-MIE and validate that its the same for each mode independent of truncation
      // Implement offset

      // Cleanup
      delete pcaptr;
      std::cout << "Everything is done. Have a nice day." << std::endl;
      break;
    }
    case config::tasks::SP:
    { // singlepoint
      std::size_t i(0u);
      auto sp_energies_fn = coords::output::filename("_SP", ".txt");
      std::ofstream sp_estr(sp_energies_fn, std::ios_base::out);
      if (!sp_estr) throw std::runtime_error("Cannot open '" +
        sp_energies_fn + "' to write SP energies.");
      sp_estr << std::setw(16) << "#";
      short_ene_stream_h(coords, sp_estr, 16);
      sp_estr << std::setw(16) << 't';
      sp_estr << '\n';
      i = 0;
      for (auto const& pes : *ci)
      {
        using namespace std::chrono;
        coords.set_xyz(pes.structure.cartesian, true);
        auto start = high_resolution_clock::now();
        coords.e();
        auto tim = duration_cast<duration<double>>
          (high_resolution_clock::now() - start);
        std::cout << "Structure " << ++i << " (" << tim.count() << " s)" << '\n';
        short_ene_stream(coords, sp_estr, 16);
        sp_estr << std::setw(16) << tim.count() << '\n';
        coords.e_head_tostream_short(std::cout);
        coords.e_tostream_short(std::cout);
      }
      break;
    }
    case config::tasks::GRAD:
    {
      // calculate gradient
      std::size_t i(0u);
      std::ofstream gstream(coords::output::filename("_GRAD", ".txt").c_str());
      for (auto const& pes : *ci)
      {
        coords.set_xyz(pes.structure.cartesian);
        coords.g();
        std::cout << "Structure " << ++i << '\n';
        coords.e_head_tostream_short(std::cout);
        coords.e_tostream_short(std::cout);
        coords.energyinterface()->print_G_tinkerlike(gstream);
      }
      break;
    }
    case config::tasks::HESS:
    {
      // calculate hessian matrix
      coords.e_head_tostream_short(std::cout);
      std::size_t i(0u);
      std::ofstream gstream(coords::output::filename("_HESS", ".txt").c_str());
      for (auto const& pes : *ci)
      {
        coords.set_xyz(pes.structure.cartesian);
        coords.h();
        std::cout << "Structure " << ++i << '\n';
        coords.e_tostream_short(std::cout);
        coords.h_tostream(gstream);
      }
      break;
    }
    case config::tasks::LOCOPT:
    {
      // local optimization
      auto lo_structure_fn = coords::output::filename("_LOCOPT");
      auto lo_energies_fn = coords::output::filename("_LOCOPT", ".txt");
      optimization::perform_locopt(coords, *ci, lo_structure_fn, lo_energies_fn);
      break;
    }
    case config::tasks::TS:
    {
      // Gradient only tabu search
      std::cout << Config::get().coords.equals;
      std::cout << "-------------------------------------------------\n";
      std::cout << Config::get().optimization.global;
      std::cout << "-------------------------------------------------\n";
      if (Config::get().optimization.global.pre_optimize)
      {
        startopt::apply(coords, ci->PES());
      }
      optimization::global::optimizers::tabuSearch gots(coords, ci->PES());
      gots.run(Config::get().optimization.global.iterations, true);
      gots.write_range("_TS");
      break;
    }
    case config::tasks::MC:
    {
      // MonteCarlo Simulation
      std::cout << Config::get().coords.equals;
      std::cout << "-------------------------------------------------\n";
      std::cout << Config::get().optimization.global;
      std::cout << "-------------------------------------------------\n";
      if (Config::get().optimization.global.pre_optimize)
      {
        startopt::apply(coords, ci->PES());
      }
      optimization::global::optimizers::monteCarlo mc(coords, ci->PES());
      mc.run(Config::get().optimization.global.iterations, true);
      mc.write_range("_MCM");
      break;
    }
    case config::tasks::GRID:
    {
      // Grid Search
      std::cout << Config::get().coords.equals;
      std::cout << "-------------------------------------------------\n";
      std::cout << Config::get().optimization.global;
      std::cout << "-------------------------------------------------\n";
      optimization::global::optimizers::main_grid mc(coords, ci->PES(),
        Config::get().optimization.global.grid.main_delta);
      mc.run(Config::get().optimization.global.iterations, true);
      mc.write_range("_GRID");
      break;
    }
    case config::tasks::INTERNAL:
    {
      // Explicitly shows CAST conversion to internal coordiantes
      // Beware when chaning this, PCA-task depend on this output and need to be adjusted accordingly.
      for (auto const& pes : *ci)
      {
        coords.set_xyz(pes.structure.cartesian);
        std::cout << coords::output::formats::zmatrix(coords);
        std::size_t const TX(coords.atoms().mains().size());
        for (std::size_t i(0U); i < TX; ++i)
        {
          std::size_t const j(coords.atoms().intern_of_main_idihedral(i));
          std::size_t const bound_intern(coords.atoms(j).ibond());
          std::size_t const angle_intern(coords.atoms(j).iangle());
          std::cout << "Main " << i << " along " << coords.atoms(bound_intern).i_to_a();
          std::cout << " and " << coords.atoms(angle_intern).i_to_a();
          std::cout << " : " << coords.main(i) << '\n';
        }

        for (auto const& e : coords.main()) std::cout << e << '\n';

      }
      break;
    }
    case config::tasks::DIMER:
    {
      // Dimer method
      coords.e_head_tostream_short(std::cout);
      std::size_t i(0U);
      std::ofstream dimerstream(coords::output::filename("_DIMERTRANS").c_str(), std::ios_base::out);
      for (auto const& pes : *ci)
      {
        coords.set_xyz(pes.structure.cartesian);
        coords.o();
        dimerstream << coords;
        std::cout << "Pre-Dimer Minimum" << ++i << '\n';
        coords.e_tostream_short(std::cout);
        optimization::global::CoordsOptimizationTS cts(&coords);
        cts.dimermethod_dihedral();
        dimerstream << coords;
        std::cout << "Dimer Transition" << i << '\n';
        coords.e_tostream_short(std::cout);
        coords.o();
        dimerstream << coords;
        std::cout << "Post-Dimer Minimum" << i << '\n';
        coords.e_tostream_short(std::cout);
      }
      break;
    }
    case config::tasks::MD:
    {
      // Molecular Dynamics Simulation
      if (Config::get().md.pre_optimize) coords.o();
      md::simulation mdObject(coords);
      mdObject.run();
      break;
    }
    case config::tasks::FEP:
    {
      // Free energy perturbation
      md::simulation mdObject(coords);
      mdObject.fepinit();
      mdObject.run();
      mdObject.feprun();
      break;
    }
    case config::tasks::UMBRELLA:
    {
      // Umbrella Sampling
      Config::set().md.umbrella = true;
      if (Config::get().md.pre_optimize) coords.o();
      md::simulation mdObject(coords);
      mdObject.umbrella_run();
      break;
    }
    case config::tasks::PMF_IC_PREP:
    {
      auto outfile = coords::output::filename("_PMF_IC", ".csv");
      auto splinefile = coords::output::filename("_SPLINE", ".csv");
      pmf_ic_prep p(coords, *ci, outfile, splinefile);
      p.run();
      break;
    }
    case config::tasks::STARTOPT:
    {
      // Preoptimization
      //std::cout << "PreApply.\n";
      startopt::apply(coords, ci->PES());
      //std::cout << "PostApply.\n";
      std::ofstream gstream(coords::output::filename("_SO").c_str());
      for (auto const& pes : ci->PES())
      {
        //std::cout << "PreSet.\n";
        coords.set_pes(pes, true);
        //std::cout << "PostSet.\n";
        gstream << coords;
      }
      if (coords.check_for_crashes() == false)
      {
        std::cout << "WARNING! Atoms are crashed. You probably don't want to use the output structure!\n";
      }
      break;
    }
    case config::tasks::GOSOL:
    { // Combined Solvation + Global Optimization
      std::cout << Config::get().startopt.solvadd;
      std::cout << "-------------------------------------------------\n";
      std::cout << Config::get().coords.equals;
      std::cout << "-------------------------------------------------\n";
      std::cout << Config::get().optimization.global;
      std::cout << "-------------------------------------------------\n";
      startopt::preoptimizers::GOSol sopt(coords, ci->PES());
      sopt.run(Config::get().startopt.solvadd.maxNumWater);
      break;
    }
    case config::tasks::NEB:
    {
      std::ptrdiff_t counter = 0;
      std::vector<coords::Representation_3D> input_pathway;
      coords::Representation_3D start_struc, final_struc;
      ptrdiff_t image_connect = ptrdiff_t(Config::get().neb.CONNECT_NEB_NUMBER);

      for (auto const& pes : *ci)
      {
        coords.set_xyz(pes.structure.cartesian);
        coords.mult_struc_counter++;
        if (Config::get().neb.COMPLETE_PATH)
        {
          input_pathway.push_back(pes.structure.cartesian);
        }
        else if (!Config::get().neb.MULTIPLE_POINTS)
        {
          neb nobj(&coords);
          nobj.preprocess(counter);
        }
      }
      if (Config::get().neb.COMPLETE_PATH && !(Config::get().neb.MULTIPLE_POINTS))
      {
        neb nobj(&coords);
        nobj.preprocess(input_pathway, counter);
      }
      else if ((Config::get().neb.MULTIPLE_POINTS))
      {
        for (size_t i = 0; i < (input_pathway.size() - 1); ++i)
        {
          start_struc = input_pathway[i];
          final_struc = input_pathway[i + 1];
          neb nobj(&coords);
          nobj.preprocess(counter, image_connect, counter, start_struc, final_struc, true);
        }
      }
      break;
    }
    case config::tasks::PATHOPT:
    {
      std::ptrdiff_t counter = 0;
      for (auto const& pes : *ci)
      {
        coords.set_xyz(pes.structure.cartesian);
        coords.mult_struc_counter++;
        neb nobj(&coords);
        nobj.preprocess(counter);
        pathx Pobj(&nobj, &coords);
        Pobj.pathx_ini();
      }
      break;
    }
    case config::tasks::PATHSAMPLING:
    {
      coords::Coordinates const coord_obj(coords);
      path_perp path_perpobj(&coords);
      path_perpobj.pathx_ini();
      break;
    }
    case config::tasks::ALIGN:
    {
      /*
       * THIS TASK ALIGNES A SIMULATION TRAJECTORY
       *
       * This task will perform a translational- and rotational fit of the conformations
       * obtained from a molecular simulation according to Kabsch's method.
       * Furthermore, molecular distance meassures may be computed afterwards
       *
       * Now even fancier through OpenMP magic
       */
      alignment(ci, coords);
      std::cout << "Everything is done. Have a nice day." << std::endl;
      break;
    }
    case config::tasks::WRITE_TINKER:
    {
      std::ofstream gstream(coords::output::filename("", ".arc").c_str());
      for (std::size_t i = 0; i < ci->size(); ++i)
      {
        auto&& temporaryPESpoint = ci->PES()[i].structure.cartesian;
        coords.set_xyz(temporaryPESpoint);
        gstream << coords::output::formats::tinker(coords);
      }
      break;
    }
    case config::tasks::WRITE_XYZ:
    {
      std::ofstream gstream(coords::output::filename("", ".xyz").c_str());
      for (std::size_t i = 0; i < ci->size(); ++i)
      {
        auto&& temporaryPESpoint = ci->PES()[i].structure.cartesian;
        coords.set_xyz(temporaryPESpoint);
        gstream << coords::output::formats::xyz_cast(coords);
      }
      break;
    }
    case config::tasks::WRITE_PDB:
    {
      std::cout << "Only the first structure of the input trajectory is written as PDB." << std::endl;
      std::ofstream pdbstream(coords::output::filename("", ".pdb").c_str());
      auto out_pdb = coords::output::formats::pdb(coords);
      out_pdb.preparation();
      pdbstream << out_pdb;
      break;
    }
    case config::tasks::FIND_AS:
    {
      find_as(coords, "find_as.txt");
      break;
    }
    case config::tasks::WRITE_GAUSSVIEW:
    {
      std::cout << "Only the first structure of the input trajectory is written as a Gaussview file." << std::endl;
      std::ofstream gstream(coords::output::filename("", ".gjf").c_str());
      auto out_gaussview = coords::output::formats::gaussview(coords);
      gstream << out_gaussview;
      break;
    }

    case config::tasks::MOVE_TO_ORIGIN:
    {
      if (Config::get().stuff.moving_mode == 1) {
        coords.move_all_by(-coords.center_of_geometry());
      }
      else if (Config::get().stuff.moving_mode == 2) {
        coords.move_all_by(-coords.center_of_mass());
      }
      else throw std::runtime_error("invalid moving mode!");
      std::ofstream gstream(coords::output::filename("", ".arc").c_str());
      gstream << coords::output::formats::tinker(coords);
      break;
    }
    case config::tasks::MODIFY_SK_FILES:
    {
      auto pairs = find_pairs(coords);
      for (auto& p : pairs) modify_file(p);
      break;
    }
    case config::tasks::PCAgen:
    {
      /**
       * THIS TASK PERFORMS PRINCIPAL COMPONENT ANALYSIS ON A SIMULATION TRAJECTORY
       *
       * This task will perform a principal component analysis (PCA) on a molecular simulation
       * trajectory. Prior translational- and rotational fit of the conformations
       * obtained is possible. Options can be specified in the INPUTFILE.
       *
       * Further processing can be done via PCAproc - Task
       */

       // Create empty pointer since we do not know yet if PCA eigenvectors etc.
       // will be generated from coordinates or read from file
      pca::PrincipalComponentRepresentation* pcaptr = nullptr;

      if (Config::get().PCA.two_traj)   // compare two trajectories
      {
        // get eigenvectors of combined trajectory
        coords::Coordinates coords(ci->read(Config::get().PCA.second_traj_name));
        pcaptr = new pca::PrincipalComponentRepresentation();
        pcaptr->generateCoordinateMatrix(ci, coords);
        pcaptr->generatePCAEigenvectorsFromCoordinates();

        // apply eigenvectors on first trajectory and write out histogram
        std::unique_ptr<coords::input::format> ci1(coords::input::new_format());
        coords::Coordinates coords1(ci1->read(Config::get().general.inputFilename));
        pcaptr->generateCoordinateMatrix(ci1, coords1);
        pcaptr->generatePCAModesFromPCAEigenvectorsAndCoordinates();
        pcaptr->writePCAModesFile("pca_modes_1.dat");
        pcaptr->writeHistogrammedProbabilityDensity("pca_histogrammed_1.dat");

        // apply eigenvectors on second trajectory and write out histogram
        std::unique_ptr<coords::input::format> ci2(coords::input::new_format());
        coords::Coordinates coords2(ci2->read(Config::get().PCA.second_traj_name));
        pcaptr->generateCoordinateMatrix(ci2, coords2);
        pcaptr->generatePCAModesFromPCAEigenvectorsAndCoordinates();
        pcaptr->writePCAModesFile("pca_modes_2.dat");
        pcaptr->writeHistogrammedProbabilityDensity("pca_histogrammed_2.dat");
      }
      else    // "normal" PCA
      {
        // Create new PCA eigenvectors and modes
        if (!Config::get().PCA.pca_read_modes && !Config::get().PCA.pca_read_vectors)
        {
          pcaptr = new pca::PrincipalComponentRepresentation(ci, coords);
          pcaptr->writePCAModesFile("pca_modes.dat");
          pcaptr->writePCAModesBinaryFile("pca_modes.cbf");
        }
        // Read modes and eigenvectors from (properly formated) file "pca_modes.dat"
        else if (Config::get().PCA.pca_read_modes && Config::get().PCA.pca_read_vectors)
        {
          
          if (Config::get().PCA.pca_read_binary)
          {
            pcaptr = new pca::PrincipalComponentRepresentation();
            pcaptr->readBinary("pca_modes.cbf", true, true, true, true);
          }
          else
          {
            pcaptr = new pca::PrincipalComponentRepresentation("pca_modes.dat");
            pcaptr->generateCoordinateMatrix(ci, coords);  // this is necessary in case of truncated coordinates
          }
        }
        else
        {
          pcaptr = new pca::PrincipalComponentRepresentation();
          // Read PCA-Modes from file but generate new eigenvectors from input coordinates (I think this doesn't make much sense???)
          if (Config::get().PCA.pca_read_modes)
          {
            pcaptr->generateCoordinateMatrix(ci, coords);
            pcaptr->generatePCAEigenvectorsFromCoordinates();
            if (Config::get().PCA.pca_read_binary)
            {
              pcaptr->readBinary("pca_modes.cbf", false, false, false, true);
            }
            else
            {
              pcaptr->readModes("pca_modes.dat");
            }
            
          }
          // Read PCA-Eigenvectors from file but generate new modes using the eigenvectors
          // and the input coordinates
          else if (Config::get().PCA.pca_read_vectors)
          {
            pcaptr->generateCoordinateMatrix(ci, coords);

            if (Config::get().PCA.pca_read_binary)
            {
              pcaptr->readBinary("pca_modes.cbf", true, true, false, false);
            }
            else
            {
              pcaptr->readEigenvectors("pca_modes.dat");

              pcaptr->readEigenvalues("pca_modes.dat");
            }
            
            pcaptr->generatePCAModesFromPCAEigenvectorsAndCoordinates();
          }
        }

        // If modes or vectors have changed, write them to new file
        if (Config::get().PCA.pca_read_modes != Config::get().PCA.pca_read_vectors) 
        {
          pcaptr->writePCAModesFile("pca_modes_new.dat");
          pcaptr->writePCAModesBinaryFile("pca_modes_new.cbf");
        }

        // Create Histograms
        // ATTENTION: This function read from Config::PCA
        pcaptr->writeHistogrammedProbabilityDensity("pca_histogrammed.dat");

        // Write Stock's Delta, see DOI 10.1063/1.2746330
        // ATTENTION: This function read from Config::PCA
        pcaptr->writeStocksDelta("pca_stocksdelta.dat");

      }

      // Cleanup
      delete pcaptr;
      std::cout << "Everything is done. Have a nice day." << std::endl;
      break;
    }
    case config::tasks::PCAproc:
    {
      /**
       * THIS TASK PERFORMS Processing of previously obtained PRINCIPAL COMPONENTs
       * To be precise, it will write out the structures coresponding to user specified PC-Ranges.
       * see also: Task PCAgen
       */
      pca::ProcessedPrincipalComponentRepresentation pcaproc("pca_modes.dat");
      pcaproc.determineStructures(ci, coords);
      pcaproc.writeDeterminedStructures(coords);
      std::cout << "Everything is done. Have a nice day." << std::endl;
      break;
    }
    case config::tasks::ENTROPY:
    {
      /**
      * THIS TASK PERFORMS CONFIGURATIONAL ENTROPY CALCULATIONS ON A SIMULATION TRAJECTORY
      *
      * This task will perform verious configurational or conformational entropy calculations
      * on a molecular simualtion trajectory. Prior translational- and rotational fit of the
      * conformations obtained is possible. Options can be specified in the INPUTFILE
      *
      */

      // Create TrajectoryMatrixRepresentation
      // This is actually quite elaborate and involves many steps
      // If cartesians are desired they will always be massweightend
      // If internals are desired they will always be transformed
      // to a linear (i.e. not circular) coordinate space)
      // Check the proceedings for more details
      const TrajectoryMatrixRepresentation * representation_massweighted = nullptr; 
      const TrajectoryMatrixRepresentation* representation_raw = nullptr;
      if (Config::get().entropy.useCartesianPCAmodes)
      {
        representation_massweighted = new TrajectoryMatrixRepresentation("pca_modes.cbf", Config::get().entropy.entropy_start_frame_num, \
          Config::get().entropy.entropy_offset, Config::get().entropy.entropy_trunc_atoms_bool ? \
          Config::get().entropy.entropy_trunc_atoms_num : std::vector<std::size_t>());
        representation_raw = nullptr;
      }
      else
      {
        //Cartesian Coordinates are Mass-Weighted!
        representation_massweighted = new TrajectoryMatrixRepresentation(ci, coords,Config::get().entropy.entropy_start_frame_num,\
          Config::get().entropy.entropy_offset, Config::get().entropy.entropy_trunc_atoms_bool ? Config::get().entropy.entropy_trunc_atoms_num : std::vector<std::size_t>(),\
          Config::get().general.verbosity,Config::get().entropy.entropy_use_internal ? Config::get().entropy.entropy_internal_dih : std::vector<std::size_t>(),\
          Config::get().entropy.entropy_alignment ? Config::get().entropy.entropy_ref_frame_num : -1, true);
        representation_raw = new TrajectoryMatrixRepresentation(ci, coords, Config::get().entropy.entropy_start_frame_num, \
          Config::get().entropy.entropy_offset, Config::get().entropy.entropy_trunc_atoms_bool ? Config::get().entropy.entropy_trunc_atoms_num : std::vector<std::size_t>(), \
          Config::get().general.verbosity, Config::get().entropy.entropy_use_internal ? Config::get().entropy.entropy_internal_dih : std::vector<std::size_t>(), \
          Config::get().entropy.entropy_alignment ? Config::get().entropy.entropy_ref_frame_num : -1, false);
      }

      const TrajectoryMatrixRepresentation& repr_mw = *representation_massweighted;

      const entropyobj entropyobj_mw(repr_mw);
      const kNN_NORM norm = static_cast<kNN_NORM>(Config::get().entropy.knnnorm);
      const kNN_FUNCTION func = static_cast<kNN_FUNCTION>(Config::get().entropy.knnfunc);

      for (size_t u = 0u; u < Config::get().entropy.entropy_method.size(); u++)
      {
        int m = (int)Config::get().entropy.entropy_method[u];
        // Karplus' method
        if (m == 1 || m == 0)
        {
          /*double entropy_value = */repr_mw.karplus();
        }
        // Knapp's method, marginal
        if (m == 2)
        {
          Matrix_Class eigenvec, eigenval, redMasses, shiftingConstants, pcaFrequencies, pcaModes;
          repr_mw.pcaTransformDraws(eigenval, eigenvec, redMasses, shiftingConstants, pcaFrequencies, pcaModes, \
            Config::get().entropy.entropy_trunc_atoms_bool ? matop::getMassVectorOfDOFs(coords, Config::get().entropy.entropy_trunc_atoms_num) : matop::getMassVectorOfDOFs(coords), \
            Config::get().entropy.entropy_temp, false);
        }
        // Knapp's method
        if (m == 3 || m == 0)
        {

          auto calcObj = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, entropyobj_mw);

          Matrix_Class eigenvec, eigenval, redMasses, shiftingConstants, pcaFrequencies, pcaModes;

          repr_mw.pcaTransformDraws(eigenval, eigenvec, redMasses, shiftingConstants, pcaFrequencies, pcaModes, \
            Config::get().entropy.entropy_trunc_atoms_bool ? matop::getMassVectorOfDOFs(coords, Config::get().entropy.entropy_trunc_atoms_num) : matop::getMassVectorOfDOFs(coords), \
            Config::get().entropy.entropy_temp, false);

          Matrix_Class stdDevPCAModes = entropy::unmassweightedStdDevFromMWPCAeigenvalues(Config::get().entropy.entropy_trunc_atoms_bool ? \
            matop::getMassVectorOfDOFs(coords, Config::get().entropy.entropy_trunc_atoms_num) : matop::getMassVectorOfDOFs(coords), eigenval, eigenvec, calcObj.getSubDims());
          calcObj.numataCorrectionsFromMI(2, eigenval, stdDevPCAModes, Config::get().entropy.entropy_temp, norm, func);

        }
        // Hnizdo's method
        if (m == 4 || m == 0)
        {
          double value = 0.;
          if(Config::get().entropy.entropy_use_massweighted)
          {
            auto calcObj = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, entropyobj_mw);
            value = calcObj.calculateFulldimensionalNNEntropyOfDraws(norm, false);
          }
          else
          {
            auto calcObj = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, entropyobj(*representation_raw));
            value = calcObj.calculateFulldimensionalNNEntropyOfDraws(norm, false);
          }
	        std::cout << std::fixed;
	        std::cout << std::setprecision(6u);
          std::cout << "Entropy value: " << value * constants::boltzmann_constant_kb_gaussian_units * constants::eV2kcal_mol * 1000.0 << " cal/(mol*K)\n " << std::endl;
        }
        // Hnizdo's method, marginal
        if (m == 5 || m == 0)
        {
          std::cout << "Commencing marginal kNN-Entropy calculation (sum of 1-dimensional entropies)." << std::endl;
          double value = 0.;
          if (Config::get().entropy.entropy_use_massweighted)
          {
            auto calcObj = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, entropyobj_mw);
            value = calcObj.calculateNN_MIExpansion(1u, norm, func, false);
          }
          else
          {
            auto calcObj = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, entropyobj(*representation_raw));
            value = calcObj.calculateNN_MIExpansion(1u, norm, func, false);
          }
	        std::cout << std::fixed;
          std::cout << std::setprecision(6u);
          std::cout << "Marginal kNN-Entropy value: " << value * constants::boltzmann_constant_kb_gaussian_units * constants::eV2kcal_mol * 1000<< " cal/(mol*K)\n " << std::endl;
        }
        // Schlitter's method
        if (m == 6 || m == 0)
        {
          
          /*double entropy_value = */repr_mw.schlitter(
            Config::get().entropy.entropy_temp, coords);
        }
        // arbitrary order MIE entropy
        if (m == 7 || m == 0)
        {
          std::cout << "Commencing ";
          std::string mie_name = "";
          if (Config::get().entropy.entropy_mie_order == 1u)
            mie_name = "1st";
          else if (Config::get().entropy.entropy_mie_order == 2u)
          {
            mie_name = "2nd";
          }
          else if (Config::get().entropy.entropy_mie_order == 3u)
          {
            mie_name = "3rd";
          }
          else
          {
            mie_name = std::to_string(Config::get().entropy.entropy_mie_order) + "th";
          }
          std::cout << mie_name << " Order MIE kNN-Entropy calculation." << std::endl;
          double value = 0.;
          if (Config::get().entropy.entropy_use_massweighted)
          {
            auto calcObj = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, entropyobj_mw);
            value = calcObj.calculateNN_MIExpansion(Config::get().entropy.entropy_mie_order, norm, func, false);
          }
          else
          {
            auto calcObj = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, entropyobj(*representation_raw));
            value = calcObj.calculateNN_MIExpansion(Config::get().entropy.entropy_mie_order, norm, func, false);
          }
          std::cout << std::fixed;
          std::cout << std::setprecision(6u);
	        std::cout << mie_name << " Order MIE kNN-Entropy value: " << value * constants::boltzmann_constant_kb_gaussian_units * constants::eV2kcal_mol * 1000.0 << " cal/(mol*K)\n " << std::endl;
        }
        // Empirical Gaussian from Std of Samples
        if (m == 8 || m == 0)
        {
          std::cout << "Commencing empirical gaussian entropy calculation." << std::endl;
          double value = 0.;
          if (Config::get().entropy.entropy_use_massweighted)
          {
            auto calcObj = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, entropyobj_mw);
            value = calcObj.empiricalGaussianEntropy();
          }
          else
          {
            auto calcObj = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, entropyobj(*representation_raw));
            value = calcObj.empiricalGaussianEntropy();
          }
          std::cout << "Empirical gaussian entropy value: " << value * constants::boltzmann_constant_kb_gaussian_units * constants::eV2kcal_mol << " kcal/(mol*K)\n " << std::endl;
        }
        // Custom kNN with purged modes
        if (m == 9 || m == 0)
        {
          if (Config::get().entropy.entropy_use_massweighted)
          {
            entropyobj curTinker = entropyobj(entropyobj_mw);
            const double curTemp = Config::get().entropy.entropy_temp;
            //
            // 1. calculate pca modes
            // establish frequencies where equipartition is not valid
            // calculate their entropy
            // remove them from draw matrix
            Matrix_Class eigenvec, eigenval, redMasses, shiftingConstants, pcaFrequencies, pcaModes;
            repr_mw.pcaTransformDraws(eigenval, eigenvec, redMasses, shiftingConstants, pcaFrequencies, pcaModes, \
              Config::get().entropy.entropy_trunc_atoms_bool ? matop::getMassVectorOfDOFs(coords, Config::get().entropy.entropy_trunc_atoms_num) : matop::getMassVectorOfDOFs(coords), \
              curTemp, false);
            //
            Matrix_Class quantum_entropy(pcaFrequencies.rows(), 1u);
            Matrix_Class classical_entropy(pcaFrequencies.rows(), 1u);
            const Matrix_Class pcaModes_t = transposed(pcaModes);
            std::vector<double> andersonDarlingValues = entropy::normalityCheck(pcaModes_t,pcaFrequencies.rows());
            if (Config::get().general.verbosity > 4)
            {
              std::cout << "Andersen-Darlin Test Values for each mode:\n";
              for (auto const& i : andersonDarlingValues)
                std::cout << i << " ";
              std::cout << "\n";
            }
            
            for (std::size_t i = 0; i < quantum_entropy.rows(); i++)
            {
              const double eigenval = 1.0/(pcaFrequencies(i,0u)* pcaFrequencies(i, 0u)/ constants::boltzmann_constant_kb_SI_units/curTemp);
              const double alpha_i = constants::h_bar_SI_units / (std::sqrt(constants::boltzmann_constant_kb_SI_units * curTemp) * std::sqrt(eigenval));
              const double quantumS = ((alpha_i / (std::exp(alpha_i) - 1)) - std::log(1 - std::exp(-1 * alpha_i))) * 1.380648813 * 6.02214129 * 0.239005736;
              quantum_entropy(i, 0u) = std::isnan(quantumS) ? 0. : quantumS;
              classical_entropy(i, 0u) = -1.0 * constants::N_avogadro * constants::boltzmann_constant_kb_SI_units * constants::joules2cal * (std::log(alpha_i) - 1.); 
              // The formula written HERE ABOVE NOW is correct, there is a sign error in the original paper of Knapp/numata
            }
            //
            double entropyVal = 0.;
            double entropyOfIncludedDimsInQHA = 0.;
            double accumulateShiftings = 0.;
            for (int i = pcaFrequencies.rows() - 1; i >= 0 ; i=i-1)
            {
              if (Config::get().entropy.purgeModesInCompositeProcedure == std::vector<size_t>())
              {
                //std::cout << "Considering mode " << i << " : " << pcaFrequencies(i, 0u) / constants::speed_of_light_cm_per_s << "\n";
                constexpr double type1error = 0.01;
                if (!entropy::isModeInClassicalLimit(pcaFrequencies(i, 0u), curTemp))
                {
                  //std::cout << "Shedding row: " << i << "\n";
                  pcaModes.shed_row(i);
                  if (Config::get().general.verbosity > 3)
                  {
                    std::cout << "Excluded mode " << i << " as it is a quantum mode. Treating it quasi-harmonically...\n";
                    std::cout << "Quasi-Harmonic-Entropy: " << quantum_entropy(i, 0u) << " cal/(mol*K)\n";

                  }
                  entropyVal += quantum_entropy(i, 0u);
                }
                else if (entropy::isModeNormalAtGivenLevel(andersonDarlingValues.at(i), type1error, entropyobj_mw.numberOfDraws) )
                {
                  //std::cout << "Shedding row: " << i << "\n";
                  pcaModes.shed_row(i);

                  if (Config::get().general.verbosity > 3)
                  {
                    std::cout << "Excluded mode " << i << " as it is gaussian at type I error: " << type1error << ". Treating it quasi-harmonically...\n";
                    std::cout << "Quasi-Harmonic-Entropy: " << quantum_entropy(i, 0u) << " cal/(mol*K)\n";

                  }
                  entropyVal += quantum_entropy(i, 0u);
                }
                else
                {
                  //Mode is treated using kNN
                  accumulateShiftings += shiftingConstants(i, 0u);
                  if (Config::get().general.verbosity > 3)
                    std::cout << "Add shift: " << shiftingConstants(i, 0u) << "\n";
                  //std::cout << "Add classical_entropy: " << classical_entropy(i, 0u) << "\n";
                  //std::cout << "Add quantum_entropy: " << quantum_entropy(i, 0u) << "\n";
                  entropyOfIncludedDimsInQHA += quantum_entropy(i, 0u);
                }
              }
              else if(std::any_of(Config::get().entropy.purgeModesInCompositeProcedure.begin(), Config::get().entropy.purgeModesInCompositeProcedure.end(), [&](const size_t& elem) { return elem == i; }))
              {
                pcaModes.shed_row(i);

                if (Config::get().general.verbosity > 3)
                {
                  std::cout << "Excluded mode " << i << " as requested by user input...\n";
                  std::cout << "Quasi-Harmonic-Entropy: " << quantum_entropy(i, 0u) << " cal/(mol*K)\n";
                }
                entropyVal += quantum_entropy(i, 0u);
              }
              else
              {
                //Mode is treated using kNN
                accumulateShiftings += shiftingConstants(i, 0u);
                if (Config::get().general.verbosity > 3)
                  std::cout << "Add shift: " << shiftingConstants(i, 0u) << "\n";
                //std::cout << "Add classical_entropy: " << classical_entropy(i, 0u) << "\n";
                //std::cout << "Add quantum_entropy: " << quantum_entropy(i, 0u) << "\n";
                entropyOfIncludedDimsInQHA += quantum_entropy(i, 0u);
              }
            }
            // 
            // perform quasi-harmonic scaling
            // run harmoizedScaling
            // run kNN
            // 
            // establish additive entropies
            //
            std::cout << "Entropy of remaining modes in Quasi-Harmonic Approximation: " << entropyOfIncludedDimsInQHA << " cal/(mol*K)\n";
            std::cout << "Entropy of quasi-harmonicly treated modes: " << entropyVal << " cal/(mol*K)\n";
            std::cout << "Accumulated Shiftings: " << accumulateShiftings << "\n";
            calculatedentropyobj const calcObj = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, entropyobj_mw);
            const double kNNentropyVal = calcObj.calculateNN(transposed(pcaModes), norm, false, kNN_FUNCTION::HNIZDO);
            std::cout << std::fixed;
            std::cout << "Full-Dimensional kNN entropy of remaining modes: " << kNNentropyVal + accumulateShiftings << " cal/(mol*K)\n";
            std::cout << "Full-Dimensional kNN entropy of all modes (composite scheme): " << kNNentropyVal + accumulateShiftings + entropyVal << " cal/(mol*K)\n";
            std::cout << std::scientific;
            //std::cout << "Shifting Constants:\n" << shiftingConstants << "\n";
            //std::cout << "~~~~~~~~~" << std::endl;

            const entropyobj calcObj2 = entropyobj(transposed(pcaModes), pcaModes.rows(), pcaModes.cols(),false);
            calculatedentropyobj calcObj3 = calculatedentropyobj(Config::get().entropy.entropy_method_knn_k, calcObj2);
            const double marginalkNN = calcObj3.calculateNN_MIExpansion(1u, norm, func, false);
            // Shiftings valid for spatial entropy in SI units
            std::cout << std::fixed;
            std::cout << "1MIE kNN entropy of remaining modes: " << marginalkNN + accumulateShiftings << " cal / (mol * K)\n";
            std::cout << "1MIE kNN entropy of all modes (composite scheme): " << marginalkNN + accumulateShiftings + entropyVal << " cal / (mol * K)\n";
            std::cout << std::scientific;
            
            //
            const double kNNentropyValArdakani = calcObj.calculateNN(transposed(pcaModes), norm, true, kNN_FUNCTION::HNIZDO);
            std::cout << std::fixed;
            std::cout << "Full-Dimensional kNN entropy of remaining modes with Ardakani's correction: " << kNNentropyValArdakani + accumulateShiftings << " cal/(mol*K)\n";
            std::cout << "Full-Dimensional kNN entropy of all modes with Ardakani's correction (composite scheme): " << kNNentropyValArdakani + accumulateShiftings + entropyVal << " cal/(mol*K)\n";
            std::cout << std::scientific;
            //
            //
            Matrix_Class tempPCA = transposed(pcaModes);
            const coords::float_type harmScaleParam = entropy::harmonizedScaling(tempPCA);
            const double kNNentropyValScaled = calcObj.calculateNN(tempPCA, norm, false, kNN_FUNCTION::HNIZDO);
            const double kNN_harm = -harmScaleParam + kNNentropyValScaled + accumulateShiftings;
            std::cout << std::fixed;
            std::cout << "Full-Dimensional kNN entropy of remaining modes with Variance-Scaling approach: " << kNN_harm << " cal/(mol*K)\n";
            std::cout << "Full-Dimensional kNN entropy of all modes with Variance-Scaling approach (composite scheme): " << kNN_harm + entropyVal << " cal/(mol*K)\n";
            std::cout << std::scientific;

            // To-Do:
            // --- Check Massweighting in SI
            // --- Check Massweighting in AU
            // --- Check no Massweighting
          }
          else
          {
            throw std::logic_error("Massweighting is necessary for this entropy estimation protocoll. Aborting.");
          }
          //std::cout << std::fixed;
          //std::cout << std::setprecision(6u);
          //std::cout << "Entropy value: " << value * constants::boltzmann_constant_kb_gaussian_units * constants::eV2kcal_mol * 1000.0 << " cal/(mol*K)\n " << std::endl;
        }
      }

      if (representation_raw != nullptr)
      {
        delete representation_raw;
      }
      if (representation_massweighted != nullptr)
      {
        delete representation_massweighted;
      }
        
      std::cout << "Everything is done. Have a nice day." << std::endl;
      break;
    }
    case config::tasks::REMOVE_EXPLICIT_WATER:
    {
      /**
      * THIS TASK REMOVES EXPLICIT WATER FROM STRUCTURES AND WRITES THE TRUNCATED STRUCTURES TO FILE
      *
      */

      std::ofstream out(coords::output::filename("_noexplwater").c_str(), std::ios::app);
      std::string* hold_str = new std::string[ci->size()];
#ifdef _OPENMP
      auto const n_omp = static_cast<std::ptrdiff_t>(ci->size());
#pragma omp parallel for firstprivate(coords) shared(hold_str)
      for (std::ptrdiff_t iter = 0; iter < n_omp; ++iter)
#else
      for (std::size_t iter = 0; iter < ci->size(); ++iter)
#endif
      {
        auto holder = ci->PES()[iter].structure.cartesian;
        coords.set_xyz(holder);

        std::vector<size_t> atomsToBePurged;
        coords::Atoms truncAtoms;
        coords::Representation_3D positions;
        for (size_t i = 0u; i < coords.atoms().size(); i++)
        {
          coords::Atom atom(coords.atoms().atom(i));
          if (atom.number() != 8u && atom.number() != 1u)
          {
            truncAtoms.add(atom);
            positions.push_back(coords.xyz(i));
          }
          else if (atom.number() == 1u)
          {
            // Check if hydrogen is bound to something else than Oxygen
            bool checker = true;
            for (size_t j = 0u; j < atom.bonds().size(); j++)
            {
              if (coords.atoms().atom(atom.bonds()[j]).number() == 8u) checker = false;
            }
            if (checker)
            {
              truncAtoms.add(atom);
              positions.push_back(coords.xyz(i));
            }
          }
          else if (atom.number() == 8u)
          {
            //checker checks if only hydrogens are bound to this current oxygen
            bool checker = true;
            for (size_t j = 0u; j < atom.bonds().size(); j++)
            {
              if (coords.atoms().atom(atom.bonds()[j]).number() != 1u) checker = false;
            }
            if (!checker)
            {
              truncAtoms.add(atom);
              positions.push_back(coords.xyz(i));
              for (auto const& bond : atom.bonds())
              {
                if (coords.atoms().atom(bond).number() == 1u)
                {
                  truncAtoms.add(coords.atoms().atom(bond));
                  positions.push_back(coords.xyz(bond));
                }
              }
            }
          }
        }
        coords::Coordinates newCoords;
        coords::PES_Point x(positions);
        newCoords.init_in(truncAtoms, x);
        std::stringstream temporaryStringstream;
        temporaryStringstream << newCoords;
        hold_str[iter] = temporaryStringstream.str();
      }
      for (size_t i = 0; i < ci->size(); i++)

      {
        out << hold_str[i];
      }
      break;
    }
    case config::tasks::SCAN2D:
    {
      auto scan = std::make_shared<Scan2D>(coords);
      scan->execute_scan();
      break;
    }
    case config::tasks::XB_EXCITON_BREAKUP:
    {
      /**
      * THIS TASK SIMULATES THE EXCITON_BREAKUP ON AN
      * INTERFACE OF TWO ORGANIC SEMICONDUCTORS:
      * (AT THE MOMENT ONLY ORGANIC SEMICONDUCTOR/FULLERENE INTERFACE)
      * NEEDS SPECIALLY PREPEARED INPUT
      */

      //XB::exciton_breakup(Config::get().exbreak.pscnumber, Config::get().exbreak.nscnumber, Config::get().exbreak.interfaceorientation, Config::get().exbreak.masscenters, 
      //		 Config::get().exbreak.nscpairrates, Config::get().exbreak.pscpairexrates, Config::get().exbreak.pscpairchrates, Config::get().exbreak.pnscpairrates);
      XB::ExcitonBreakup breakup(Config::get().exbreak.masscenters, Config::get().exbreak.nscpairrates, Config::get().exbreak.pscpairexrates, Config::get().exbreak.pscpairchrates, Config::get().exbreak.pnscpairrates);
      std::vector<size_t> startingPoints;
      std::random_device rd;
      std::mt19937 engine(rd());
      std::uniform_int_distribution<std::size_t> unirand(1u, breakup.getTotalNumberOfMonomers());
      for (std::size_t i = 0u; i < 251u; i++)
      {
        startingPoints.push_back(unirand(engine));
      }
      breakup.run(Config::get().exbreak.interfaceorientation, 1u, 25000);
      breakup.analyseResults(1u);
      break;
    }
    case config::tasks::XB_INTERFACE_CREATION:
    {
      /**
      * THIS TASK CREATES A NEW COORDINATE SET FROM TWO PRECURSORS
      */
      //creating second coords object
      std::unique_ptr<coords::input::format> add_strukt_uptr(coords::input::additional_format());
      coords::Coordinates add_coords(add_strukt_uptr->read(Config::get().interfcrea.icfilename));
      coords::Coordinates newCoords(coords);


      newCoords = periodicsHelperfunctions::interface_creation(Config::get().interfcrea.icaxis, Config::get().interfcrea.icdist, coords, add_coords);

      coords = newCoords;

      std::ofstream new_structure(Config::get().general.outputFilename, std::ios_base::out);
      new_structure << coords;

      break;
    }
    case config::tasks::XB_CENTER:
    {
      /**
      * THIS  TASK CALCULATES THE CENTERS OF MASSES FOR ALL MONOMERS IN THE STRUCTURE AND IF WANTED GIVES STRUCTURE FILES FOR DIMERS
      * WITHIN A DEFINED DISTANCE BETWEEN THE MONOMERS
      */
      center(coords);
      break;
    }
    case config::tasks::XB_COUPLINGS:
    {
      /**
      * THIS  TASK CALCULATES THE CENTERS OF MASSES FOR ALL MONOMERS IN THE STRUCTURE AND IF WANTED GIVES STRUCTURE FILES FOR DIMERS
      * WITHIN A DEFINED DISTANCE BETWEEN THE MONOMERS
      */
      couplings::coupling coup;
      coup.calculateAndWriteToFile();

      break;
    }
    case config::tasks::LAYER_DEPOSITION:
    {
      //Generating layer with random defects
      coords::Coordinates newCoords(coords);
      coords::Coordinates inp_add_coords(coords);
      coords::Coordinates add_coords;

      for (auto& pes : *ci)
      {
        newCoords.set_xyz(pes.structure.cartesian);
        newCoords = periodicsHelperfunctions::delete_random_molecules(coords, Config::get().layd.del_amount);
        pes = newCoords.pes();
      }
      newCoords.set_xyz(ci->structure(0u).structure.cartesian);
      coords = newCoords;

      std::ofstream output(Config::get().general.outputFilename, std::ios_base::out);
      output << coords;


      for (std::size_t i = 0u; i < 1; i++) //this loops purpose is to ensure mdObject1 is destroyed before further changes to coords happen and the destructor goes bonkers. Not elegant but does the job.
      {
        //Molecular Dynamics Simulation
        if (Config::get().md.pre_optimize) coords.o();
        Config::set().md.num_steps = Config::get().layd.steps;
        md::simulation mdObject1(coords);
        mdObject1.run();
      }

      for (std::size_t i = 1; i < Config::get().layd.amount; i++)
      {
        add_coords = inp_add_coords;
        add_coords = periodicsHelperfunctions::delete_random_molecules(add_coords, Config::get().layd.del_amount);

        for (auto& pes : *ci)
        {
          newCoords.set_xyz(pes.structure.cartesian);
          newCoords = periodicsHelperfunctions::interface_creation(Config::get().layd.laydaxis, Config::get().layd.layddist, coords, add_coords);
          pes = newCoords.pes();
        }
        newCoords.set_xyz(ci->structure(0u).structure.cartesian);
        coords = newCoords;


        for (std::size_t j = 0; j < (coords.size() - add_coords.size()); j++)//fix all atoms already moved by md
        {
          coords.fix(j, true);
        }

        // Molecular Dynamics Simulation
        if (Config::get().md.pre_optimize) coords.o();
        Config::set().md.num_steps = Config::get().layd.steps;
        md::simulation mdObject2(coords);
        mdObject2.run();
      }

      std::size_t mon_amount_type1 = coords.molecules().size();//save number of molecules of first kind for later use in replacement

      //option if a heterogenous structure shall be created
      if (Config::get().layd.hetero_option == true)
      {
        std::unique_ptr<coords::input::format> sec_strukt_uptr(coords::input::additional_format());
        coords::Coordinates sec_coords(sec_strukt_uptr->read(Config::get().layd.layd_secname));
        coords::Coordinates add_sec_coords;

        for (std::size_t i = 0; i < Config::get().layd.sec_amount; i++)
        {
          add_sec_coords = sec_coords;
          add_sec_coords = periodicsHelperfunctions::delete_random_molecules(add_sec_coords, Config::get().layd.sec_del_amount);

          for (auto& pes : *ci)
          {
            newCoords.set_xyz(pes.structure.cartesian);
            newCoords = periodicsHelperfunctions::interface_creation(Config::get().layd.laydaxis, Config::get().layd.sec_layddist, coords, add_sec_coords);
            pes = newCoords.pes();
          }
          newCoords.set_xyz(ci->structure(0u).structure.cartesian);
          coords = newCoords;

          for (std::size_t j = 0; j < (coords.size() - add_sec_coords.size()); j++)//fix all atoms already moved by md
          {
            coords.fix(j, true);
          }

          // Molecular Dynamics Simulation
          if (Config::get().md.pre_optimize) coords.o();
          Config::set().md.num_steps = Config::get().layd.het_steps;
          md::simulation mdObject3(coords);
          mdObject3.run();
        }

        
      }

      //option if a third structure shall be added
      if (Config::get().layd.ter_option == true)
      {
        std::unique_ptr<coords::input::format> ter_strukt_uptr(coords::input::additional_format());
        coords::Coordinates ter_coords(ter_strukt_uptr->read(Config::get().layd.layd_tername));
        coords::Coordinates add_ter_coords;

        for (std::size_t i = 0; i < Config::get().layd.ter_amount; i++)
        {
          add_ter_coords = ter_coords;
          add_ter_coords = periodicsHelperfunctions::delete_random_molecules(add_ter_coords, Config::get().layd.ter_del_amount);

          for (auto& pes : *ci)
          {
            newCoords.set_xyz(pes.structure.cartesian);
            newCoords = periodicsHelperfunctions::interface_creation(Config::get().layd.laydaxis, Config::get().layd.ter_layddist, coords, add_ter_coords);
            pes = newCoords.pes();
          }
          newCoords.set_xyz(ci->structure(0u).structure.cartesian);
          coords = newCoords;

          for (std::size_t j = 0; j < (coords.size() - add_ter_coords.size()); j++)//fix all atoms already moved by md
          {
            coords.fix(j, true);
          }

          // Molecular Dynamics Simulation
          if (Config::get().md.pre_optimize) coords.o();
          Config::set().md.num_steps = Config::get().layd.ter_steps;
          md::simulation mdObject4(coords);
          mdObject4.run();
        }


      }

      //option if monomers in structure shall be replaced
      if (Config::get().layd.replace == true)
      {
        std::unique_ptr<coords::input::format> add_strukt_uptr(coords::input::additional_format());
        coords::Coordinates add_coords1(add_strukt_uptr->read(Config::get().layd.reference1));
        std::unique_ptr<coords::input::format> add_strukt_uptr2(coords::input::additional_format());
        coords::Coordinates add_coords2(add_strukt_uptr2->read(Config::get().layd.reference2));

        coords = monomerManipulation::replaceMonomers(coords, add_coords1, add_coords2, mon_amount_type1);
      }

      //std::ofstream output(Config::get().general.outputFilename, std::ios_base::out);
      output << coords;
      break;
    }
    case config::tasks::EXCITONDIMER:
    {
      exciD::dimexc(Config::get().exbreak.masscenters, Config::get().exbreak.couplings, Config::get().exbreak.pscnumber, Config::get().exbreak.nscnumber,
        Config::get().exbreak.interfaceorientation, Config::get().exbreak.startingPscaling, Config::get().exbreak.nbrStatingpoins);
      break;
    }
    case config::tasks::GET_MONOMERS:
    {
      /** This task separates a strucure into its monomerstructures
      */
      getMonomers(coords);
      break;
    }
    default:
    {

    }
    }
#ifdef USE_PYTHON
    Py_Finalize(); //  close python
#endif 

    // stop and print task and execution time
    std::cout << '\n' << "Task " << config::task_strings[Config::get().general.task];
    std::cout << " took " << task_timer << " to complete.\n";
    std::cout << "Execution of " << config::Programname << " (" << config::Version << ")";
    std::cout << " ended after " << exec_timer << '\n';

    //////////////////////////
    //                      //
    //       EXCEPTION      //
    //       HANDLING       //
    //                      //
    //////////////////////////
#ifndef CAST_DEBUG_DROP_EXCEPTIONS
    }
#if defined COMPILEX64 || defined __LP64__ || defined _WIN64 

  catch (std::bad_alloc&)
  {
    std::cout << "Memory allocation failure. Input structure probably too large.\n";
  }
#else
  catch (std::bad_alloc&)
  {
    std::cout << "Memory allocation failure. CAST probably ran out of memory. Try using 64bit compiled " << config::Programname << " instead.\n";
  }
#endif
  catch (std::exception & e)
  {
    std::cout << "Congratulations! The execution of " << config::Programname << " failed successfully. \n";
    std::cout << "Error: " << e.what() << '\n';
  }
#endif
#ifdef _MSC_VER 
  // make window stay open in debug session on windows
  if (IsDebuggerPresent()) std::system("pause");
#endif
  return 0;
  }
#endif
