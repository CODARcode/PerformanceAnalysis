#include <random>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include <chimbuko/pserver/GlobalAnomalyStats.hpp>

using namespace chimbuko;


TEST(TestAggregateFuncStats, AggregationAndJSON){  
  //Generate some stats representing the inclusive and exclusive runtimes collected over 2 different io steps
  RunStats incl1, incl2, excl1, excl2;

  std::mt19937 gen(1234);
  std::normal_distribution<> dexcl(100,20);
  std::normal_distribution<> dincl_add(200,100);

  for(int i=0;i<100;i++){
    double e1 = dexcl(gen);
    double i1 = e1 + dincl_add(gen);

    double e2 = dexcl(gen);
    double i2 = e2 + dincl_add(gen);
    
    incl1.push(i1);
    excl1.push(e1);
    
    incl2.push(i2);
    excl2.push(e2);
  }

  int pid = 99;
  int fid = 77;
  std::string fname = "my_func";

  int nanom1 = 23; //anoms on first step
  int nanom2 = 44; //anoms on second step

  AggregateFuncStats fstats(pid,fid,fname);
  fstats.add(nanom1, incl1, excl1);
  fstats.add(nanom2, incl2, excl2);

  EXPECT_EQ(fstats.get_func(), fname);
  EXPECT_EQ(fstats.get_func_anomaly().count(), 2);
  EXPECT_EQ(fstats.get_func_anomaly().accumulate(), nanom1 + nanom2);
  
  RunStats incl_sum = incl1 + incl2;
  RunStats excl_sum = excl1 + excl2;
  EXPECT_EQ(fstats.get_inclusive().get_stat_values(), incl_sum.get_stat_values());
  EXPECT_EQ(fstats.get_exclusive().get_stat_values(), excl_sum.get_stat_values());

  nlohmann::json fstats_j = fstats.get_json();
  EXPECT_EQ(fstats_j["app"], pid);
  EXPECT_EQ(fstats_j["fid"], fid);
  EXPECT_EQ(fstats_j["name"], fname);
  EXPECT_EQ(fstats_j["stats"]["count"],  2);
  EXPECT_EQ(fstats_j["stats"]["accumulate"],  nanom1+nanom2);
  EXPECT_EQ(fstats_j["inclusive"], incl_sum.get_json());
  EXPECT_EQ(fstats_j["exclusive"], excl_sum.get_json());
}




