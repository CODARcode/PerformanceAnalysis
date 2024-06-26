#include "gtest/gtest.h"
#include "../../../unit_test_common.hpp"

#include <chimbuko/modules/performance_analysis/pserver/GlobalAnomalyStats.hpp>
#include <chimbuko/core/util/string.hpp>
#include<random>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

TEST(TestGlobalAnomalyStats, UpdateFuncStat){  
  GlobalAnomalyStats stats;

  //Generate some stats representing the inclusive and exclusive runtimes collected over 2 different io steps
  RunStats incl1(true), incl2(true), excl1(true), excl2(true); //accumulate total time over all ranks

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

  const AggregateFuncStats & fstats = stats.get_func_stats(pid, fid);

  EXPECT_EQ(fstats.get_func(), fname);
  EXPECT_EQ(fstats.get_func_anomaly().count(), 2);
  EXPECT_EQ(fstats.get_func_anomaly().accumulate(), nanom1 + nanom2);
  
  RunStats incl_sum = incl1 + incl2;
  RunStats excl_sum = excl1 + excl2;
  EXPECT_EQ(fstats.get_inclusive().get_stat_values(), incl_sum.get_stat_values());
  EXPECT_EQ(fstats.get_exclusive().get_stat_values(), excl_sum.get_stat_values());

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

  const AggregateAnomalyData & astats = stats.get_anomaly_stat_container(app, rank);
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


bool near(const RunStats &a, const RunStats &b, double tol){
  return fabs( a.mean() - b.mean() ) < tol && fabs( a.stddev() - b.stddev() ) < tol;
}


TEST(TestGlobalAnomalyStats, Combine){  
  //Generate fake funcstats
  RunStats incl1(true), incl2(true), excl1(true), excl2(true); //accumulate total time over all ranks

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
  int fid2 = 33;
  std::string fname2= "other_func";

  int nanom1 = 23; //anoms on first step
  int nanom2 = 44; //anoms on second step

  //Check combine with 2 different fids works
  {
    GlobalAnomalyStats a1;
    a1.update_func_stat(pid, fid, fname, nanom1, incl1, excl1);

    GlobalAnomalyStats a2;
    a2.update_func_stat(pid, fid2, fname2, nanom2, incl2, excl2);

    GlobalAnomalyStats a_full;
    a_full.update_func_stat(pid, fid, fname, nanom1, incl1, excl1);
    a_full.update_func_stat(pid, fid2, fname2, nanom2, incl2, excl2);

    GlobalAnomalyStats a_comb(a1);
    EXPECT_EQ(a_comb, a1); //test copy constructor
    a_comb += a2;

    const AggregateFuncStats &a_full_fid = a_full.get_func_stats(pid,fid);
    const AggregateFuncStats &a_comb_fid = a_comb.get_func_stats(pid,fid);
    
    EXPECT_EQ(near(a_full_fid.get_func_anomaly(),a_comb_fid.get_func_anomaly(),1e-8), true);
    EXPECT_EQ(near(a_full_fid.get_inclusive(),a_comb_fid.get_inclusive(),1e-8), true);
    EXPECT_EQ(near(a_full_fid.get_exclusive(),a_comb_fid.get_exclusive(),1e-8), true);

    const AggregateFuncStats &a_full_fid2 = a_full.get_func_stats(pid,fid2);
    const AggregateFuncStats &a_comb_fid2 = a_comb.get_func_stats(pid,fid2);

    EXPECT_EQ(near(a_full_fid2.get_func_anomaly(),a_comb_fid2.get_func_anomaly(),1e-8), true);
    EXPECT_EQ(near(a_full_fid2.get_inclusive(),a_comb_fid2.get_inclusive(),1e-8), true);
    EXPECT_EQ(near(a_full_fid2.get_exclusive(),a_comb_fid2.get_exclusive(),1e-8), true);
  }

  //Same fid, multiple data
  {
    GlobalAnomalyStats a1;
    a1.update_func_stat(pid, fid, fname, nanom1, incl1, excl1);

    GlobalAnomalyStats a2;
    a2.update_func_stat(pid, fid, fname, nanom2, incl2, excl2);

    GlobalAnomalyStats a_full;
    a_full.update_func_stat(pid, fid, fname, nanom1, incl1, excl1);
    a_full.update_func_stat(pid, fid, fname, nanom2, incl2, excl2);

    GlobalAnomalyStats a_comb(a1);
    EXPECT_EQ(a_comb, a1); //test copy constructor
    a_comb += a2;

    const AggregateFuncStats &a_full_fid = a_full.get_func_stats(pid,fid);
    const AggregateFuncStats &a_comb_fid = a_comb.get_func_stats(pid,fid);
    
    EXPECT_EQ(near(a_full_fid.get_func_anomaly(),a_comb_fid.get_func_anomaly(),1e-8), true);
    EXPECT_EQ(near(a_full_fid.get_inclusive(),a_comb_fid.get_inclusive(),1e-8), true);
    EXPECT_EQ(near(a_full_fid.get_exclusive(),a_comb_fid.get_exclusive(),1e-8), true);
  }

  //Generate fake AnomalyData
  int rank1 = 2, rank2 = 55;

  AnomalyData anom_rank1_1(pid, rank1, 13, 14, 15, 16);
  for(int i=0;i<16;i++) anom_rank1_1.add_outlier_score(M_PI*(i+1));

  AnomalyData anom_rank1_2(pid, rank1, 13, 14, 15, 17);
  for(int i=0;i<17;i++) anom_rank1_2.add_outlier_score(M_PI*(i*i+1));

  AnomalyData anom_rank2_1(pid, rank2, 19, 20, 21, 22);
  for(int i=0;i<22;i++) anom_rank2_1.add_outlier_score(M_PI*M_PI*(i+3));

  {
    //Check data from 2 different ranks incorporated correctly
    GlobalAnomalyStats a1;
    a1.update_anomaly_stat(anom_rank1_1);

    GlobalAnomalyStats a2;
    a2.update_anomaly_stat(anom_rank2_1);

    GlobalAnomalyStats a_full;
    a_full.update_anomaly_stat(anom_rank1_1);
    a_full.update_anomaly_stat(anom_rank2_1);
    
    GlobalAnomalyStats a_comb(a1);
    EXPECT_EQ(a_comb, a1);
    a_comb += a2;
    
    const AggregateAnomalyData & a_full_rank1 = a_full.get_anomaly_stat_container(pid, rank1);
    const AggregateAnomalyData & a_full_rank2 = a_full.get_anomaly_stat_container(pid, rank2);

    const AggregateAnomalyData & a_comb_rank1 = a_comb.get_anomaly_stat_container(pid, rank1);
    const AggregateAnomalyData & a_comb_rank2 = a_comb.get_anomaly_stat_container(pid, rank2);

    EXPECT_EQ( near(a_full_rank1.get_stats(), a_comb_rank1.get_stats(), 1e-8), true );
    EXPECT_EQ( near(a_full_rank2.get_stats(), a_comb_rank2.get_stats(), 1e-8), true );
    EXPECT_EQ( a_comb_rank1.get_data()->size(), 1 );
    EXPECT_EQ( a_comb_rank1.get_data()->front(), anom_rank1_1 );
    EXPECT_EQ( a_comb_rank2.get_data()->size(), 1 );
    EXPECT_EQ( a_comb_rank2.get_data()->front(), anom_rank2_1 );
  }

  {
    //Check data from multiple data sets on a single rank
    GlobalAnomalyStats a1;
    a1.update_anomaly_stat(anom_rank1_1);

    GlobalAnomalyStats a2;
    a2.update_anomaly_stat(anom_rank1_2);

    GlobalAnomalyStats a_full;
    a_full.update_anomaly_stat(anom_rank1_1);
    a_full.update_anomaly_stat(anom_rank1_2);
    
    GlobalAnomalyStats a_comb(a1);
    EXPECT_EQ(a_comb, a1);
    a_comb += a2;
    
    const AggregateAnomalyData & a_full_rank1 = a_full.get_anomaly_stat_container(pid, rank1);
    const AggregateAnomalyData & a_comb_rank1 = a_comb.get_anomaly_stat_container(pid, rank1);

    EXPECT_EQ( near(a_full_rank1.get_stats(), a_comb_rank1.get_stats(), 1e-8), true );
    EXPECT_EQ( a_comb_rank1.get_data()->size(), 2 );
    EXPECT_EQ( a_comb_rank1.get_data()->front(), anom_rank1_1 );
    EXPECT_EQ( a_comb_rank1.get_data()->back(), anom_rank1_2 );

    //Check merge_and_flush
    GlobalAnomalyStats a_comb2(a1);
    GlobalAnomalyStats a2_cp(a2);
    a_comb2.merge_and_flush(a2_cp);
    EXPECT_EQ(a_comb2, a_comb);
    const AggregateAnomalyData & a2_cp_rank1 = a2_cp.get_anomaly_stat_container(pid, rank1);
    EXPECT_EQ(a2_cp_rank1.get_data()->size(), 0);
  }

  


}
