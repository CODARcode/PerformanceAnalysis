#include <random>
#include "gtest/gtest.h"
#include "../../../unit_test_common.hpp"

#include <chimbuko/modules/performance_analysis/pserver/AggregateFuncAnomalyMetrics.hpp>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

TEST(AggregateFuncAnomalyMetrics, Aggregation){  
  int pid = 0;
  int rid = 1;
  int steps[2] = {2,3};
  int fid = 3;
  int tid=10;
  std::string func = "myfunc";

  //Events on first step
  eventID id1(rid,steps[0],1), id2(rid,steps[0],2);
  
  ExecData_t 
    e1(id1, pid, rid, tid, fid, func, 100, 200),
    e2(id2, pid, rid, tid, fid, func, 300, 450);
  
  e1.set_label(-1);
  e1.set_outlier_score(3.14);
  e2.set_label(-1);
  e2.set_outlier_score(6.28);

  FuncAnomalyMetrics met1(fid,func);
  met1.add(e1);
  met1.add(e2);

  //Events on the second step
  eventID id3(rid,steps[1],1), id4(rid,steps[1],2);
  
  ExecData_t 
    e3(id3, pid, rid, tid, fid, func, 500, 700),
    e4(id4, pid, rid, tid, fid, func, 750, 1000);
  
  e3.set_label(-1);
  e3.set_outlier_score(9.99);
  e4.set_label(-1);
  e4.set_outlier_score(12.11);

  FuncAnomalyMetrics met2(fid,func);
  met2.add(e3);
  met2.add(e4);

  //Stats should combine info from both steps
  RunStats expect_count(true); //count is anomalies per io step
  expect_count.push(2);
  expect_count.push(2);
  
  RunStats expect_score(true);
  expect_score.push(e1.get_outlier_score());
  expect_score.push(e2.get_outlier_score());
  expect_score.push(e3.get_outlier_score());
  expect_score.push(e4.get_outlier_score());

  RunStats expect_severity(true);
  expect_severity.push(e1.get_outlier_severity());
  expect_severity.push(e2.get_outlier_severity());
  expect_severity.push(e3.get_outlier_severity());
  expect_severity.push(e4.get_outlier_severity());

  AggregateFuncAnomalyMetrics agg(pid, rid, fid, func);
  EXPECT_EQ(agg.get_pid(), pid);
  EXPECT_EQ(agg.get_rid(), rid);
  EXPECT_EQ(agg.get_fid(), fid);
  EXPECT_EQ(agg.get_func(), func);

  //Add the data
  //Check the first and last event ts are correct
  agg.add(met1, 33, 100, 200);
  EXPECT_EQ(agg.get_first_event_ts(), 100);
  EXPECT_EQ(agg.get_last_event_ts(), 200);
  EXPECT_EQ(agg.get_first_io_step(),33);
  EXPECT_EQ(agg.get_last_io_step(),33);
  agg.add(met2, 44, 50, 300); //even if ts not given in order
  EXPECT_EQ(agg.get_first_event_ts(), 50);
  EXPECT_EQ(agg.get_last_event_ts(), 300);
  EXPECT_EQ(agg.get_first_io_step(),33);
  EXPECT_EQ(agg.get_last_io_step(),44);
  

  EXPECT_EQ(agg.get_count_stats().get_json().dump(4), expect_count.get_json().dump(4));
  EXPECT_EQ(compare( agg.get_score_stats(), expect_score, 1e-10), true);
  EXPECT_EQ(compare( agg.get_severity_stats(), expect_severity, 1e-10), true);
}

bool near(const RunStats &a, const RunStats &b, double tol){
  return fabs( a.mean() - b.mean() ) < tol && fabs( a.stddev() - b.stddev() ) < tol;
}

TEST(AggregateFuncAnomalyMetrics, Combination){
  RunStats stats1,stats2,stats3,stats4;
  for(int i=0;i<100;i++){
    stats1.push(M_PI * (i+1));
    stats2.push(M_PI*M_PI * (i+1));
    stats3.push(M_PI*M_PI*M_PI * (i+1));
    stats4.push(M_PI*M_PI*M_PI*M_PI * (i+1));
  }

  int pid = 111;
  int fid1=99, fid2=33;
  std::string fname1="func1", fname2="func2";
  int count1=122,count2=999;

  //fid1 data 1
  FuncAnomalyMetrics fdata_fid1_1;
  fdata_fid1_1.set_score(stats1);
  fdata_fid1_1.set_severity(stats2);
  fdata_fid1_1.set_count(count1);
  fdata_fid1_1.set_fid(fid1);
  fdata_fid1_1.set_func(fname1);

  //fid1 data 2
  FuncAnomalyMetrics fdata_fid1_2;
  fdata_fid1_2.set_score(stats3);
  fdata_fid1_2.set_severity(stats4);
  fdata_fid1_2.set_count(count2);
  fdata_fid1_2.set_fid(fid1);
  fdata_fid1_2.set_func(fname1);

  //fid2 data 1
  FuncAnomalyMetrics fdata_fid2_1;
  fdata_fid2_1.set_score(stats3);
  fdata_fid2_1.set_severity(stats4);
  fdata_fid2_1.set_count(count1);
  fdata_fid2_1.set_fid(fid2);
  fdata_fid2_1.set_func(fname2);

  int rid1=22, rid2=77;
  int step1=34, step2=98, step3=101, step4=200;
  int first1 = 1024, first2=2944, first3=8888, first4=10000;
  int last1 = 2888, last2=3004, last3=4000, last4=4232;

  {
    //Check fail if rank indices don't match
    AggregateFuncAnomalyMetrics d1(pid, rid1, fid1, fname1);
    AggregateFuncAnomalyMetrics d2(pid, rid2, fid1, fname1);
    bool err = false;
    try{
      d1 += d2;
    }catch(const std::exception &e){
      err = true;
    }
    ASSERT_TRUE(err);
  }
  {
    //Check fail if func indices don't match
    AggregateFuncAnomalyMetrics d1(pid, rid1, fid1, fname1);
    AggregateFuncAnomalyMetrics d2(pid, rid1, fid2, fname1);
    bool err = false;
    try{
      d1 += d2;
    }catch(const std::exception &e){
      err = true;
    }
    ASSERT_TRUE(err);
  }
  {
    //Check fail if func names don't match
    AggregateFuncAnomalyMetrics d1(pid, rid1, fid1, fname1);
    AggregateFuncAnomalyMetrics d2(pid, rid1, fid1, fname2);
    bool err = false;
    try{
      d1 += d2;
    }catch(const std::exception &e){
      err = true;
    }
    ASSERT_TRUE(err);
  }

  {
    //Check merge for which first entry timestamps first and last are min and max of pair, respectively and step is earlier
    AggregateFuncAnomalyMetrics d1(pid, rid1, fid1, fname1);
    d1.add(fdata_fid1_1, step1, first1, last4);

    AggregateFuncAnomalyMetrics d2(pid, rid1, fid1, fname1);
    d2.add(fdata_fid1_2, step2, first2, last3);
    
    AggregateFuncAnomalyMetrics dall(pid, rid1, fid1, fname1);
    dall.add(fdata_fid1_1, step1, first1, last4);
    dall.add(fdata_fid1_2, step2, first2, last3);

    AggregateFuncAnomalyMetrics dcomb(d1);
    ASSERT_EQ(dcomb, d1);
    dcomb += d2;

    EXPECT_EQ(dcomb.get_first_event_ts(), first1);
    EXPECT_EQ(dcomb.get_last_event_ts(), last4);
    EXPECT_EQ(dcomb.get_first_io_step(), step1);
    EXPECT_EQ(dcomb.get_last_io_step(), step2);

    EXPECT_EQ(dcomb.get_first_event_ts(), dall.get_first_event_ts());
    EXPECT_EQ(dcomb.get_last_event_ts(), dall.get_last_event_ts());
    EXPECT_EQ(dcomb.get_first_io_step(), dall.get_first_io_step());
    EXPECT_EQ(dcomb.get_last_io_step(), dall.get_last_io_step());
    
    EXPECT_TRUE( near(dcomb.get_count_stats(), dall.get_count_stats(), 1e-8) );
    EXPECT_TRUE( near(dcomb.get_score_stats(), dall.get_score_stats(), 1e-8) );
    EXPECT_TRUE( near(dcomb.get_severity_stats(), dall.get_severity_stats(), 1e-8) );
  }


  {
    //Check merge for which second entry timestamps first and last are min and max of pair, respectively and step is earlier
    AggregateFuncAnomalyMetrics d1(pid, rid1, fid1, fname1);
    d1.add(fdata_fid1_1, step2, first2, last3);

    AggregateFuncAnomalyMetrics d2(pid, rid1, fid1, fname1);
    d2.add(fdata_fid1_2, step1, first1, last4);
    
    AggregateFuncAnomalyMetrics dall(pid, rid1, fid1, fname1);
    dall.add(fdata_fid1_1, step2, first2, last3);
    dall.add(fdata_fid1_2, step1, first1, last4);

    AggregateFuncAnomalyMetrics dcomb(d1);
    ASSERT_EQ(dcomb, d1);
    dcomb += d2;

    EXPECT_EQ(dcomb.get_first_event_ts(), first1);
    EXPECT_EQ(dcomb.get_last_event_ts(), last4);
    EXPECT_EQ(dcomb.get_first_io_step(), step1);
    EXPECT_EQ(dcomb.get_last_io_step(), step2);

    EXPECT_EQ(dcomb.get_first_event_ts(), dall.get_first_event_ts());
    EXPECT_EQ(dcomb.get_last_event_ts(), dall.get_last_event_ts());
    EXPECT_EQ(dcomb.get_first_io_step(), dall.get_first_io_step());
    EXPECT_EQ(dcomb.get_last_io_step(), dall.get_last_io_step());
    
    EXPECT_TRUE( near(dcomb.get_count_stats(), dall.get_count_stats(), 1e-8) );
    EXPECT_TRUE( near(dcomb.get_score_stats(), dall.get_score_stats(), 1e-8) );
    EXPECT_TRUE( near(dcomb.get_severity_stats(), dall.get_severity_stats(), 1e-8) );
  }





}
