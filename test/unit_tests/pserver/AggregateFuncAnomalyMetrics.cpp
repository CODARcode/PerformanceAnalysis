#include <random>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include <chimbuko/pserver/AggregateFuncAnomalyMetrics.hpp>

using namespace chimbuko;

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
