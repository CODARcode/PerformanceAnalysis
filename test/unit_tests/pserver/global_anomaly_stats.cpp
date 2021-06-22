#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include <chimbuko/pserver/global_anomaly_stats.hpp>
#include <chimbuko/util/string.hpp>
#include<random>

using namespace chimbuko;

TEST(TestGlobalAnomalyStats, UpdateFuncStat){  
  GlobalAnomalyStats stats;

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

  stats.update_func_stat(pid, fid, fname, nanom1, incl1, excl1);
  stats.update_func_stat(pid, fid, fname, nanom2, incl2, excl2);

  const GlobalAnomalyStats::FuncStats & fstats = stats.get_func_stats(pid, fid);

  EXPECT_EQ(fstats.func, fname);
  EXPECT_EQ(fstats.func_anomaly.count(), 2);
  EXPECT_EQ(fstats.func_anomaly.accumulate(), nanom1 + nanom2);
  
  RunStats incl_sum = incl1 + incl2;
  RunStats excl_sum = excl1 + excl2;
  EXPECT_EQ(fstats.inclusive.get_stat_values(), incl_sum.get_stat_values());
  EXPECT_EQ(fstats.exclusive.get_stat_values(), excl_sum.get_stat_values());

  nlohmann::json fstats_json = stats.collect_func_data();

  EXPECT_EQ( fstats_json.is_array(), true );
  EXPECT_EQ( fstats_json.size(), 1 );

  EXPECT_EQ( fstats_json[0]["app"], pid );
  EXPECT_EQ( fstats_json[0]["fid"], fid );
  EXPECT_EQ( fstats_json[0]["name"], fname );
}

TEST(TestGlobalAnomalyStats, UpdateAnomalyStat){  
  GlobalAnomalyStats stats;
  
  int app = 0;
  int rank = 1;
  std::string stat_id = stringize("%d:%d", app, rank);
  
  int step1 = 0;
  int min_ts1 = 0;
  int max_ts1 = 1000;
  int n_anomalies1 = 23;

  AnomalyData d1(app, rank, step1, min_ts1, max_ts1, n_anomalies1);
  
  int step2= 1;
  int min_ts2 = 1000;
  int max_ts2 = 2000;
  int n_anomalies2 = 42;

  AnomalyData d2(app, rank, step2, min_ts2, max_ts2, n_anomalies2);
  
  stats.update_anomaly_stat(d1);
  stats.update_anomaly_stat(d2);

  const AnomalyStat & astats = stats.get_anomaly_stat_container(app, rank);
  RunStats astatsr = astats.get_stats();
  
  EXPECT_EQ(astatsr.count(), 2);
  EXPECT_EQ(astatsr.accumulate(), n_anomalies1 + n_anomalies2);

  std::list<AnomalyData> const* adata = astats.get_data();
  EXPECT_EQ(adata->size(), 2);
  EXPECT_EQ( *adata->begin(), d1 );
  EXPECT_EQ( *std::next(adata->begin(),1), d2 );

  nlohmann::json astats_j = stats.collect_stat_data();
  EXPECT_EQ(astats_j.is_array(), true );
  EXPECT_EQ(astats_j.size(), 1 );
  EXPECT_EQ(astats_j[0]["key"], stat_id);
  EXPECT_EQ(astats_j[0]["stats"]["accumulate"], n_anomalies1 + n_anomalies2);
  EXPECT_EQ(astats_j[0]["data"].size(), 2 );

  //Check data was flushed
  adata = astats.get_data();
  EXPECT_EQ(adata->size(), 0);

  //Check that collect_stat_data only includes data if the number of anomalies seen since last collect > 0
  int step3= 2;
  int min_ts3 = 2000;
  int max_ts3 = 3000;
  int n_anomalies3 = 0;

  AnomalyData d3(app, rank, step3, min_ts3, max_ts3, n_anomalies3);
  
  stats.update_anomaly_stat(d3);
  
  astats_j = stats.collect_stat_data();
  EXPECT_EQ(astats_j.is_array(), true );
  EXPECT_EQ(astats_j.size(), 0 );
  
  //Check that if there are multiple steps, some of which have anomalies, that the data array only contains the data with nanom > 0

  int step4= 3;
  int min_ts4 = 3000;
  int max_ts4 = 4000;
  int n_anomalies4 = 0;

  AnomalyData d4(app, rank, step4, min_ts4, max_ts4, n_anomalies4);

  int step5= 4;
  int min_ts5 = 4000;
  int max_ts5 = 5000;
  int n_anomalies5 = 10; 

  AnomalyData d5(app, rank, step5, min_ts5, max_ts5, n_anomalies5);

  stats.update_anomaly_stat(d4);
  stats.update_anomaly_stat(d5);

  astats_j = stats.collect_stat_data();
  EXPECT_EQ(astats_j.is_array(), true );
  EXPECT_EQ(astats_j.size(), 1 ); //1 rank
  EXPECT_EQ(astats_j[0]["data"].size(), 1); //only d5
  EXPECT_EQ(astats_j[0]["data"][0]["step"], step5);
}

