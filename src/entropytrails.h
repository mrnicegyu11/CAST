#pragma once
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <omp.h>
#include "matop.h"
#include <string>


/////////////////
// Some constants
/////////////////

#ifndef M_PIl
/** The constant Pi in high precision */
#define M_PIl 3.1415926535897932384626433832795029L
#endif
#ifndef M_GAMMAl
/** Euler's constant in high precision */
#define M_GAMMAl 0.5772156649015328606065120900824024L
#endif
#ifndef M_LN2l
/** the natural logarithm of 2 in high precision */
#define M_LN2l 0.6931471805599453094172321214581766L
#endif

const double pi = M_PIl;
const double e = 2.71828182845904523536028747135266249775724709369995;

/////////////////
// Auxiliary Functions
/////////////////

// Gives DiGamma Function value, via Stackoverflow
long double digammal(long double x)
{
  /* force into the interval 1..3 */
  if (x < 0.0L)
    return digammal(1.0L - x) + M_PIl / tanl(M_PIl*(1.0L - x));	/* reflection formula */
  else if (x < 1.0L)
    return digammal(1.0L + x) - 1.0L / x;
  else if (x == 1.0L)
    return -M_GAMMAl;
  else if (x == 2.0L)
    return 1.0L - M_GAMMAl;
  else if (x == 3.0L)
    return 1.5L - M_GAMMAl;
  else if (x > 3.0L)
    /* duplication formula */
    return 0.5L*(digammal(x / 2.0L) + digammal((x + 1.0L) / 2.0L)) + M_LN2l;
  else
  {
    /* Just for your information, the following lines contain
    * the Maple source code to re-generate the table that is
    * eventually becoming the Kncoe[] array below
    * interface(prettyprint=0) :
    * Digits := 63 :
    * r := 0 :
    *
    * for l from 1 to 60 do
    * 	d := binomial(-1/2,l) :
    * 	r := r+d*(-1)^l*(Zeta(2*l+1) -1) ;
    * 	evalf(r) ;
    * 	print(%,evalf(1+Psi(1)-r)) ;
    *o d :
    *
    * for N from 1 to 28 do
    * 	r := 0 :
    * 	n := N-1 :
    *
    *	for l from iquo(n+3,2) to 70 do
    *		d := 0 :
    *		for s from 0 to n+1 do
    *		 d := d+(-1)^s*binomial(n+1,s)*binomial((s-1)/2,l) :
    *		od :
    *		if 2*l-n > 1 then
    *		r := r+d*(-1)^l*(Zeta(2*l-n) -1) :
    *		fi :
    *	od :
    *	print(evalf((-1)^n*2*r)) ;
    *od :
    *quit :
    */
    static long double Kncoe[] = { .30459198558715155634315638246624251L,
      .72037977439182833573548891941219706L, -.12454959243861367729528855995001087L,
      .27769457331927827002810119567456810e-1L, -.67762371439822456447373550186163070e-2L,
      .17238755142247705209823876688592170e-2L, -.44817699064252933515310345718960928e-3L,
      .11793660000155572716272710617753373e-3L, -.31253894280980134452125172274246963e-4L,
      .83173997012173283398932708991137488e-5L, -.22191427643780045431149221890172210e-5L,
      .59302266729329346291029599913617915e-6L, -.15863051191470655433559920279603632e-6L,
      .42459203983193603241777510648681429e-7L, -.11369129616951114238848106591780146e-7L,
      .304502217295931698401459168423403510e-8L, -.81568455080753152802915013641723686e-9L,
      .21852324749975455125936715817306383e-9L, -.58546491441689515680751900276454407e-10L,
      .15686348450871204869813586459513648e-10L, -.42029496273143231373796179302482033e-11L,
      .11261435719264907097227520956710754e-11L, -.30174353636860279765375177200637590e-12L,
      .80850955256389526647406571868193768e-13L, -.21663779809421233144009565199997351e-13L,
      .58047634271339391495076374966835526e-14L, -.15553767189204733561108869588173845e-14L,
      .41676108598040807753707828039353330e-15L, -.11167065064221317094734023242188463e-15L };

    register long double Tn_1 = 1.0L;	/* T_{n-1}(x), started at n=1 */
    register long double Tn = x - 2.0L;	/* T_{n}(x) , started at n=1 */
    register long double resul = Kncoe[0] + Kncoe[1] * Tn;

    x -= 2.0L;

    for (int n = 2; n < sizeof(Kncoe) / sizeof(long double); n++)
    {
      const long double Tn1 = 2.0L * x * Tn - Tn_1;	/* Chebyshev recursion, Eq. 22.7.4 Abramowitz-Stegun */
      resul += Kncoe[n] * Tn1;
      Tn_1 = Tn;
      Tn = Tn1;
    }
    return resul;
  }
}

// entropyobj
// Contains a matrix with draws from a distribution and 
// the number of draws(iter), dimensionality(dimension)
// and standard deviation (sigma)
// 
// No calculations have been done here
//
// CURRENTLY ONLY WORKS WITH NORMAL DISTRIBUTION
class entropyobj
{
public:
  size_t iter, sigma, dimension;
  Matrix_Class matrix;
  entropyobj(Matrix_Class& matrix_, size_t& iter_, size_t& dimension_, double& sigma_) : matrix(matrix_), iter(iter_), dimension(dimension_), sigma(sigma_) {}
};


// Calculated entropy object calculates estiamted entropy
class calculatedentropyobj : public entropyobj
{
public:
  size_t k;
  double calculatedEntropyHnizdo;
  double calculatedEntropyLombardi;
  double analyticEntropy;
  calculatedentropyobj(size_t k_, entropyobj const& obj, double analyticEntropyValue = 0.) : entropyobj(obj), k(k_), calculatedEntropyHnizdo(0),
    calculatedEntropyLombardi(0.), analyticEntropy(0)
  {
    this->calculate();
  }

  void calculate()
  {
    auto PDF = [this](double x) { return (1. / sqrt(2. * pi * this->sigma * this->sigma)) * exp(-1.*(x*x) / double(2. * this->sigma * this->sigma)); };


    // Choose your destiny
    // Standard procedure: 
    // Calculate Hnizdo as well as Lombardi/Pant entropy
    bool standardprocedure = true;

    // ExtraProcedure:
    // Debug stuff
    // Calculates entropy inegral using MC 
    // with known PDF
    bool extraprocedure = true;
    // Range for MC draws
    double range = 30.;

    // Standard procedure: 
    // Calculate Hnizdo as well as Lombardi/Pant entropy
    if (standardprocedure)
    {
      // Matrix Layout after calculation:
      // First col: Drawn samples
      // Second col: k / iter * kNNdistance
      // Third col: Value of real distribution
      // Fourth col: kNN Distance

      Matrix_Class addition(matrix.rows(), 3);
      matrix.append_right(addition);

      //Neccessarry
      transpose(matrix);
      Matrix_Class copytemp = matrix;
      Matrix_Class copytemp2 = matrix;
#ifdef _OPENMP
#pragma omp parallel firstprivate(copytemp) shared(copytemp2)
     {
#endif
        float_type* buffer = new float_type[k];
#ifdef _OPENMP
        auto const n_omp = static_cast<std::ptrdiff_t>(matrix.cols());

#pragma omp for
        for (std::ptrdiff_t i = 0; i < n_omp; ++i)
#else
        for (size_t i = 0u; i < matrix.cols(); i++)
#endif
        {
          float_type holdNNdistance = sqrt(matop::entropy::knn_distance(copytemp, 1, k, 0u, i, buffer));
          copytemp2(1, i) = double(k) / double(iter) / holdNNdistance;
          copytemp2(2, i) = PDF(copytemp(0, i));
          copytemp2(3, i) = holdNNdistance;
        }
#ifdef _OPENMP
      }
#endif
      matrix = copytemp2;

      // ENTROPY according to Hnzido
      double tempsum = 0.;
      for (size_t i = 0u; i < matrix.cols(); i++)
      {
        tempsum += log(matrix(3, i));
      }
      tempsum /= double(iter);
      tempsum *= double(dimension);

      // Einschub
      // Entropy according to Lombardi
      double tempsum_kpn_alternative = tempsum + (log(pow(pi, double(dimension) / 2.)) / (tgamma(0.5 * dimension + 1)));
      tempsum_kpn_alternative += digammal(iter);
      tempsum_kpn_alternative -= digammal(k);

      tempsum += (log(iter * pow(pi, double(dimension) / 2.)) / (tgamma(0.5 * dimension + 1)));
      double tempsum2 = 0;
      if (k != 1)
      {
        for (size_t i = 1; i < k; i++)
        {
          tempsum2 += 1.0 / double(i);
        }
      }
      tempsum -= tempsum2;
      tempsum += 0.5772156649015328606065;

      //////////////
      calculatedEntropyHnizdo = tempsum; // Hnizdo Entropy
      calculatedEntropyLombardi = tempsum_kpn_alternative; // Lombardi Entropy

      //Neccessarry
      transpose(matrix);
    }
    if (extraprocedure)
    {
      transpose(matrix);

      // new try MC guess with Kahan Summation
      double drawMCvalue = 0.;
      long double drawMCvalue2 = 0;
      long double c = 0;
      for (size_t i = 0u; i < matrix.cols(); i++)
      {
        // Value of Distribution at point x
        double temp1 = PDF(matrix(0, i));
        drawMCvalue += log(temp1);

        long double y = log(temp1) - c;
        long double t = drawMCvalue2 + y;
        c = (t - drawMCvalue2) - y;
        drawMCvalue2 = t;
      }
      drawMCvalue /= -1.* double(matrix.cols());
      drawMCvalue2 /= -1* double(matrix.cols());
      std::cout << "drawMCvalue" << drawMCvalue << std::endl;

      // MC guess with uniform distribution
      // Draw Uniform and do
      // Draw samples
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_real_distribution<double> distr(-1. * range / 2.f, range / 2.f);
      std::vector<double> temp;
      temp.reserve(iter);
      for (int n = 0; n < iter; ++n) {
        double drawnnum = distr(gen);
        temp.push_back(drawnnum);
      }

      //Sort samples
      //std::stable_sort(temp.begin(), temp.end());

      long double uniformMCvalue = 0, uniformMCvalue2 = 0;
      c = 0.f;
      for (int n = 0; n < iter; ++n) 
      {
        long double gvalue;
        gvalue = PDF(temp[n]);

        long double y = gvalue * log(gvalue) - c;
        long double t = uniformMCvalue + y;
        c = (t - uniformMCvalue) - y;
        uniformMCvalue = t;
        uniformMCvalue2 += gvalue * log(gvalue);
      }
      uniformMCvalue /= double(iter);
      uniformMCvalue2 /= double(iter);
      uniformMCvalue *= -1. * range;
      uniformMCvalue2 *= -1. * range;
      std::cout << "uniformMCvalue:" << uniformMCvalue << std::endl;

    }
  }

  void writeToFile()
  {
    std::ofstream myfile;
    myfile.open(std::string("out_k" + std::to_string(k) + "_i" + std::to_string(iter) + "_s" + std::to_string(sigma) + "_d" + std::to_string(dimension) +".txt"));

    for (size_t i = 0u; i < matrix.rows(); i++)
    {
      for (size_t j = 0u; j < matrix.cols(); j++)
        myfile << std::setw(15) << std::scientific << std::setprecision(5) << matrix(i, j);

      myfile << "\n";
    }
    myfile.close();


    std::ofstream myfile2;
    myfile2.open(std::string("out_entropy_i" + std::to_string(iter) + "_s" + std::to_string(sigma) + "_d" + std::to_string(dimension) + ".txt"), std::ios::app);

    // First line is k=0 means analytical gauss entropy log(sigma * sqrt(2 * pi * e))
    // Then every line is "k", "Hnizdo entropy"
    myfile2 << std::setw(15) << std::scientific << std::setprecision(5) << "differential entropy";
    myfile2 << std::setw(15) << std::scientific << std::setprecision(5) << log(sigma * sqrt(2 * pi * e)) << "\n";

    myfile2 << std::setw(15) << std::scientific << std::setprecision(5) << "karplus entropy" << "\n";

    myfile2 << std::setw(15) << std::scientific << std::setprecision(5) << "schlitter entropy" << "\n";

    myfile2 << std::setw(15) << std::scientific << std::setprecision(5) << "hnizdo entropy";

    myfile2 << std::setw(15) << std::scientific << std::setprecision(5) << std::to_string(k);
    myfile2 << std::setw(15) << std::scientific << std::setprecision(5) << calculatedEntropyHnizdo << "\n";
    myfile2.close();
  }


};

class entropyconfig
{

private:
  // Stacking is
  std::vector<entropyobj> matvec;
  std::vector<size_t> iter;
  std::vector<double> sigma;
  std::vector<size_t> dimension;
  std::vector<size_t> k_values;
  std::vector<calculatedentropyobj> calculatedDistributions;
public:
  entropyconfig()
  {
  }

  void readConfig(int argc, char **argv)
  {
    /*
    for (int i = 0; i < argc; i++) {
      if (std::string(argv[i]).substr(0u, 2) == "k=")
      {
        size_t lastCommaFound = 0;
        while (std::string(argv[i]).substr(lastCommaFound, std::string::npos).find(",") != std::string::npos)
        {
          k_values.push_back(size_t(std::stoi(std::string(argv[i]).substr( std::max(size_t(3u), lastCommaFound), std::string(argv[i]).find(",", lastCommaFound) - std::max(size_t(3u), lastCommaFound)))));
          lastCommaFound = std::string(argv[i]).find(",", lastCommaFound) + 1;
        }
      }
      else if (std::string(argv[i]).substr(0u, 2) == "i=")
      {
          size_t lastCommaFound = 0;
          while (std::string(argv[i]).substr(lastCommaFound, std::string::npos).find(",") != std::string::npos)
          {
            iter.push_back(size_t(std::stoi(std::string(argv[i]).substr(std::max(size_t(3u), lastCommaFound), std::string(argv[i]).find(",", lastCommaFound) - std::max(size_t(3u), lastCommaFound)))));
            lastCommaFound = std::string(argv[i]).find(",", lastCommaFound) + 1;
          }
      }
      else if (std::string(argv[i]).substr(0u, 2) == "d=")
      {
          size_t lastCommaFound = 0;
          while (std::string(argv[i]).substr(lastCommaFound, std::string::npos).find(",") != std::string::npos)
          {
            dimension.push_back(size_t(std::stoi(std::string(argv[i]).substr(std::max(size_t(3u), lastCommaFound), std::string(argv[i]).find(",", lastCommaFound) - std::max(size_t(3u), lastCommaFound)))));
            lastCommaFound = std::string(argv[i]).find(",", lastCommaFound) + 1;
          }
      }
      else if (std::string(argv[i]).substr(0u, 2) == "s=")
      {
          size_t lastCommaFound = 0;
          while (std::string(argv[i]).substr(lastCommaFound, std::string::npos).find(",") != std::string::npos)
          {
            sigma.push_back(size_t(std::stod(std::string(argv[i]).substr(std::max(size_t(3u), lastCommaFound), std::string(argv[i]).find(",", lastCommaFound) - std::max(size_t(3u), lastCommaFound)))));
            lastCommaFound = std::string(argv[i]).find(",", lastCommaFound) + 1;
          }
      }
    }
    */
    k_values = Config::get().entropytrails.k;
    dimension = Config::get().entropytrails.dimension;
    sigma = Config::get().entropytrails.sigma;
    iter = Config::get().entropytrails.iteration;
  }

  void draw()
  {
    for (size_t d = 0u; d < dimension.size(); d++)
    {
      for (size_t s = 0u; s < sigma.size(); s++)
      {
        for (size_t i = 0u; i < iter.size(); i++)
        {
          Matrix_Class currentmat(iter[i]);
          // Check if draw exists
          std::ifstream myfile;
          std::string line;
          myfile.open(std::string("draw_i" + std::to_string(iter[i]) + "_s" + std::to_string(sigma[s]) + "_d" + std::to_string(dimension[d]) + ".txt"));
          if (myfile.good())
          {
            int j = 0;
            while (std::getline(myfile, line))
            {
              currentmat(j, 0) = std::stod(line);
              j++;
            }
            myfile.close();
          }
          else
          {
            myfile.close();
            // Draw samples
            std::random_device rd;
            std::mt19937 gen(rd());
            std::normal_distribution<double> distr(0, sigma[s]);
            std::vector<double> temp;
            temp.reserve(iter[i]);
            for (int n = 0; n < iter[i]; ++n) {
              double drawnnum = distr(gen);
              temp.push_back(drawnnum);
            }
            //Sort samples
            std::stable_sort(temp.begin(), temp.end());
            for (int n = 0; n < iter[i]; ++n)
              currentmat(n, 0) = temp[n];


            // Write 
            std::ofstream myfile2;
            myfile2.open(std::string("draw_i" + std::to_string(iter[i]) + "_s" + std::to_string(sigma[s]) + "_d" + std::to_string(dimension[d]) + ".txt"));

            for (size_t w = 0u; w < currentmat.rows(); w++)
            {
              myfile2 << std::setw(15) << std::scientific << std::setprecision(5) << currentmat(w);
              myfile2 << "\n";
            }
            myfile2.close();
          }
          matvec.push_back(entropyobj(currentmat, iter[i], dimension[d], sigma[s]));
        }
      }
    }
  }

  void calculateall(void)
  {
    for (auto kc : k_values)
    {
      for (auto obj : matvec)
      {
        calculatedentropyobj temp(kc, obj);
        calculatedDistributions.push_back(temp);
        std::cout << "done" << std::endl;
      }
    }
  }

  void writeall(void)
  {
    for (auto obj : calculatedDistributions)
    {
      obj.writeToFile();
    }
  }

};