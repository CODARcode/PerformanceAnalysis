#include <random>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include <chimbuko/pserver/GlobalAnomalyStats.hpp>

using namespace chimbuko;


TEST(TestAggregateFuncStats, AggregationAndJSON){  
  //Generate some stats representing the inclusive and exclusive runtimes collected over 2 different io steps
  RunStats incl1(true), incl2(true), excl1(true), excl2(true); //total runtimes should aggregate
  RunStats incl1_noac(false), incl2_noac(false), excl1_noac(false), excl2_noac(false); //versions which don't accumulate so we can test that AggregateFuncStats forces accumulate despite input settings

  std::mt19937 gen(1234);
  std::normal_distribution<> dexcl(100,20);
  std::normal_distribution<> dincl_add(200,100);

  double tsum_incl = 0, tsum_excl = 0;

  for(int i=0;i<100;i++){
    double e1 = dexcl(gen);
    double i1 = e1 + dincl_add(gen);

    double e2 = dexcl(gen);
    double i2 = e2 + dincl_add(gen);
    
    incl1.push(i1);
    excl1.push(e1);
    
    incl2.push(i2);
    excl2.push(e2);

    incl1_noac.push(i1);
    excl1_noac.push(e1);
    
    incl2_noac.push(i2);
    excl2_noac.push(e2);

    tsum_incl += i1 + i2;
    tsum_excl += e1 + e2;
  }

  int pid = 99;
  int fid = 77;
  std::string fname = "my_func";

  int nanom1 = 23; //anoms on first step
  int nanom2 = 44; //anoms on second step

  AggregateFuncStats fstats(pid,fid,fname);
  fstats.add(nanom1, incl1_noac, excl1_noac);
  fstats.add(nanom2, incl2_noac, excl2_noac);

  nlohmann::json fstats_j = fstats.get_json();
  //std::cout << fstats_j.dump(4) << std::endl;

  EXPECT_EQ(fstats.get_func(), fname);
  EXPECT_EQ(fstats.get_func_anomaly().count(), 2);
  EXPECT_EQ(fstats.get_func_anomaly().accumulate(), nanom1 + nanom2);
  
  RunStats incl_sum = incl1 + incl2;
  RunStats excl_sum = excl1 + excl2;
  EXPECT_NEAR(incl_sum.accumulate(), tsum_incl, 1e-8);
  EXPECT_NEAR(excl_sum.accumulate(), tsum_excl, 1e-8);

  RunStats::RunStatsValues incl_got = fstats.get_inclusive().get_stat_values();
  RunStats::RunStatsValues incl_expect = incl_sum.get_stat_values();
  
#define RR(A) EXPECT_NEAR(incl_got.A, incl_expect.A, 1e-8)
  RR(count);
  RR(accumulate);
  RR(minimum);
  RR(maximum);
  RR(mean);
  RR(stddev);
  RR(skewness);
  RR(kurtosis);
#undef RR
  
  RunStats::RunStatsValues excl_got = fstats.get_exclusive().get_stat_values();
  RunStats::RunStatsValues excl_expect = excl_sum.get_stat_values();

#define RR(A) EXPECT_NEAR(excl_got.A, excl_expect.A, 1e-8)
  RR(count);
  RR(accumulate);
  RR(minimum);
  RR(maximum);
  RR(mean);
  RR(stddev);
  RR(skewness);
  RR(kurtosis);
#undef RR


  EXPECT_EQ(fstats_j["app"], pid);
  EXPECT_EQ(fstats_j["fid"], fid);
  EXPECT_EQ(fstats_j["name"], fname);
  EXPECT_EQ(fstats_j["stats"]["count"],  2);
  EXPECT_EQ(fstats_j["stats"]["accumulate"],  nanom1+nanom2);
}




