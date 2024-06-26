#include "chimbuko/core/util/RunStats.hpp"
#include "chimbuko/core/util/serialize.hpp"
#include "gtest/gtest.h"
#include <cereal/archives/portable_binary.hpp>
#include <sstream>
#include "../../../unit_tests/unit_test_common.hpp"

using namespace chimbuko;

//Textbook definitions
double mean(const std::vector<double> &a){
  double r=0;
  for(double v:a) r+=v;
  return r/a.size();
}
double variance(const std::vector<double> &a, bool incl_bessel = true){
  double n = a.size();
  double r=0,r2=0;
  for(double v:a){ r+=v; r2+=v*v; }
  r = (r2/n - r/n*r/n);
  if(incl_bessel) r *= n/(n-1);  //include Bessel's correction by default
  return r;
}
double skewness(const std::vector<double> &a){
  double mu = mean(a);
  double sigma = sqrt(variance(a,false));
  double r = 0;
  for(double v:a) r += pow( (v - mu)/sigma, 3 );
  return r/a.size();
}
double kurtosis(const std::vector<double> &a){ //technically excess kurtosis
  double mu = mean(a);
  double sigma = sqrt(variance(a,false));
  double r = 0;
  for(double v:a) r += pow( (v - mu)/sigma, 4 );
  return r/a.size() - 3;
}



//Independent implementation using https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance
struct statsTest{
  double n;
  double mu;
  double M2;
  double M3;
  double M4;

  statsTest(): n(0), mu(0), M2(0), M3(0), M4(0){}
  statsTest(const std::vector<double> &v){
    n = v.size();
    mu = 0;
    M2 = 0;
    M3 = 0;
    M4 = 0;
    for(double e: v)
      mu += e;
    mu /= n;
    
    for(double e: v){
      M2 += pow(e - mu,2);
      M3 += pow(e - mu,3);
      M4 += pow(e - mu,4);
    }
      
  }

  double variance() const{ //includes Bessel's correction
    return M2/(n-1.);
  }
  double mean() const{
    return mu;
  }
  double skewness() const{
    return M3/n/pow(M2/n,3./2.);
  }
  double kurtosis() const{
    return M4/n/pow(M2/n,2) - 3;
  }

};

statsTest operator+(const statsTest &a, const statsTest &b){ 
  statsTest out;
  out.n = a.n + b.n;
  double delta = b.mu - a.mu;
  out.mu = a.mu + delta * b.n/out.n;
  out.M2 = a.M2 + b.M2 + pow(delta,2) * a.n * b.n / out.n;
  out.M3 = a.M3 + b.M3 + pow(delta,3) * a.n * b.n * (a.n - b.n) / pow(out.n,2) + 3*delta*(a.n*b.M2 - b.n*a.M2)/out.n;
  out.M4 = a.M4 + b.M4 + pow(delta,4) * a.n * b.n * (a.n*a.n - a.n*b.n + b.n*b.n)/pow(out.n,3) + 6*pow(delta,2) * (a.n*a.n*b.M2 + b.n*b.n*a.M2 ) / pow(out.n,2)
    + 4*delta*( a.n*b.M3 - b.n*a.M3 ) / out.n;
  
  return out;
}

bool compare(const statsTest &a, const statsTest &b, const double tol = 1e-12){
  bool ret = true;
#define COM(A) if(2.*fabs( a. A - b. A )/(a. A  + b. A) > tol){ std::cout << #A << " " << a. A << " " << b. A << std::endl; ret = false; }
  COM(n);
  COM(mu);
  COM(M2);
  COM(M3);
  COM(M4);
  return ret;
#undef COM
}

class RunStatsAcc: public RunStats{
public:
  RunStatsAcc(const RunStats &r): RunStats(r){}

  bool compare(const statsTest &a, const double tol = 1e-12){
    // double eta; /**< mean */
    // double rho; /**< = M2 = \sum_i (x_i - \bar x)^2 */
    // double tau; /**< = M3 = \sum_i (x_i - \bar x)^3 */
    // double phi; /**< = M4 = \sum_i (x_i - \bar x)^4 */
    
    bool ret = true;
#define COM(A,B) if(					    \
		    (fabs(a. A)<=tol && fabs(this->m_##B)>tol) ||		\
		    (fabs(this->m_##B)<=tol && fabs(a. A)>tol) ||		\
		    (fabs(a. A)>tol && fabs(this->m_##B) > tol && 2.*fabs( a. A - this->m_##B )/(a. A  + this->m_##B) > tol) \
							    ){ std::cout << #A << " " << a. A << " " << #B << " " << this-> m_##B << std::endl; ret = false; }
    COM(n,count);
    COM(mu,eta);
    COM(M2,rho);
    COM(M3,tau);
    COM(M4,phi);
    return ret;
#undef COM
  }
};

bool compare(const statsTest &a, const RunStats &b, const double tol = 1e-12){
  RunStatsAcc sb(b);
  return sb.compare(a,tol);
}


TEST(TestRunStats, TestIndependentImplementation){
  //Test that summing two RunStats is the same as if the data were collected by a single RunStats instance
  std::vector<std::vector<double> > all_vals = {
    {160,150,140,122,103,77,33,22,19,7,1},
    {77,33,22,19,7,1},
    {77,33,22,19},
    {-0.2, -0.5, 0.7, -0.4},
    {3.14,6.28,9.99,10.22},
    {1000,2000,3000,4000},
    {22,-22,22,-22}
  };
  for(auto const &vals: all_vals){
    std::vector<double> data_a, data_b;

    int na = vals.size() / 2;
    int nb = vals.size() - na;
    for(int i=0;i<na;i++)
      data_a.push_back(vals[i]);
    for(int i=na;i<na+nb;i++)
      data_b.push_back(vals[i]);
  
    for(int i=0;i<vals.size();i++){
      std::cout << vals[i] << " ";
    }
    std::cout << std::endl;

    statsTest a(data_a), b(data_b), c(vals);

    ASSERT_NEAR(c.mean(), mean(vals), 1e-3);
    ASSERT_NEAR(c.variance(), variance(vals), 1e-3);
    ASSERT_NEAR(c.skewness(), skewness(vals), 1e-3);
    ASSERT_NEAR(c.kurtosis(), kurtosis(vals), 1e-3);
    std::cout << "Full dist mean " << c.mean() << " var " << c.variance() << " skewness " << c.skewness() << " kurtosis " << c.kurtosis() << " match expected" << std::endl;
    
    statsTest sum = a + b;

    bool result = compare(c, sum, 1e-10);

    std::cout << "Result a+b: " << (result?"pass":"fail") << std::endl;

    EXPECT_EQ(result,true);
    ASSERT_NEAR(c.mean(), sum.mean(),1e-5);
    ASSERT_NEAR(c.variance(), sum.variance(),1e-5);
    ASSERT_NEAR(c.skewness(), sum.skewness(),1e-5);
    ASSERT_NEAR(c.kurtosis(), sum.kurtosis(),1e-5);
  }

}
 

TEST(TestRunStats, TestSumCombine){
  //Test that summing two RunStats is the same as if the data were collected by a single RunStats instance
  std::vector<std::vector<double> > all_vals = {
    {160,150,140,122,103,77,33,22,19,7,1},
    {77,33,22,19,7,1},
    {77,33,22,19},
    {-0.2, -0.5, 0.7, -0.4},
    {3.14,6.28,9.99,10.22},
    {1000,2000,3000,4000},
    {22,-22,22,-22}
  };
  for(auto const &vals: all_vals){
    RunStats a(true),b(true),c(true);

    int na = vals.size() / 2;
    int nb = vals.size() - na;
    for(int i=0;i<na;i++)
      a.push(vals[i]);
    for(int i=na;i<na+nb;i++)
      b.push(vals[i]);

    for(int i=0;i<vals.size();i++){
      std::cout << vals[i] << " ";
      c.push(vals[i]);
    }
    std::cout << std::endl;

    //Check against independent implementation
    std::vector<double> data_a, data_b;
    for(int i=0;i<na;i++)
      data_a.push_back(vals[i]);
    for(int i=na;i<na+nb;i++)
      data_b.push_back(vals[i]);
    
    statsTest ia(data_a), ib(data_b), ic(vals);
    
    std::cout << "Comparing distribution 'a' to independent implementation" << std::endl;
    bool result = compare(ia,a, 1e-10);
    std::cout << "Result: " << (result?"pass":"fail") << std::endl;
    ASSERT_EQ(result,true);	

    std::cout << "Comparing distribution 'b' to independent implementation" << std::endl;
    result = compare(ib,b, 1e-10);
    std::cout << "Result: " << (result?"pass":"fail") << std::endl;   
    ASSERT_EQ(result,true);

    std::cout << "Comparing distribution 'c' to independent implementation" << std::endl;
    result = compare(ic,c, 1e-10);
    std::cout << "Result: " << (result?"pass":"fail") << std::endl;
    ASSERT_EQ(result,true);    

    std::cout << "Comparing distribution 'c' moments to textbook definitions" << std::endl;
    ASSERT_NEAR(c.mean(), mean(vals), 1e-3);
    ASSERT_NEAR(c.variance(), variance(vals), 1e-3);
    ASSERT_NEAR(c.skewness(), skewness(vals), 1e-3);
    ASSERT_NEAR(c.kurtosis(), kurtosis(vals), 1e-3);
    std::cout << "Full dist mean " << c.mean() << " var " << c.variance() << " skewness " << c.skewness() << " kurtosis " << c.kurtosis() << " match expected" << std::endl;

    statsTest isum = ia+ib;
    ASSERT_EQ(compare(ic,isum,1e-10),true);
    
    RunStats sum = a + b;
    std::cout << "Comparing combined distribution 'a+b' to independent implementation" << std::endl;
    result = compare(ic,sum, 1e-10);
    std::cout << "Result: " << (result?"pass":"fail") << std::endl;
    ASSERT_EQ(result,true);


    std::cout << "Comparing combined distribution 'a+b' to 'c'" << std::endl;
    result = compare(c, sum, 1e-10);
    std::cout << "Result: " << (result?"pass":"fail") << std::endl;
    EXPECT_EQ(result,true);

    sum = b + a;
    
    std::cout << "Comparing combined distribution 'b+a' to 'c'" << std::endl;
    result = compare(c, sum, 1e-10);
    std::cout << "Result: " << (result?"pass":"fail") << std::endl;
    EXPECT_EQ(result,true);
  }
}
 


TEST(TestRunStats, serialize){
  RunStats stats;
  for(int i=0;i<100;i++) stats.push(i);

  std::string ser = stats.net_serialize();
  RunStats stats_rd;
  stats_rd.net_deserialize(ser);
  
  EXPECT_EQ(stats, stats_rd);

  //Test cereal serialization
  ser = cereal_serialize(stats);
  stats_rd.clear();
  cereal_deserialize(stats_rd, ser);

  EXPECT_EQ(stats, stats_rd);

  std::unordered_map<std::string, RunStats> cpd;
  cpd["hello"] = stats;
  ser = cereal_serialize(cpd);

  std::unordered_map<std::string, RunStats> cpd_r;
  cereal_deserialize(cpd_r, ser);
  
  EXPECT_EQ(cpd, cpd_r);


  std::unordered_map<std::string, std::unordered_map<std::string,RunStats> > cpdd;
  cpdd["hello"]["world"] = stats;
  ser = cereal_serialize(cpdd);

  std::unordered_map<std::string, std::unordered_map<std::string,RunStats> > cpdd_r;
  cereal_deserialize(cpdd_r, ser);
  
  EXPECT_EQ(cpdd, cpdd_r);


  typedef   std::unordered_map<unsigned long, std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, RunStats>>>> mapType;
  mapType m;
  m[1234]["up"]["down"]["left"] = stats;
  ser = cereal_serialize(m);

  mapType m_r;
  cereal_deserialize(m_r, ser);
  
  EXPECT_EQ(m, m_r);
}
  
  

  
