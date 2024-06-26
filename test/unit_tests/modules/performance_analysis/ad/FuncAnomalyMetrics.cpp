#include<chimbuko/modules/performance_analysis/ad/FuncAnomalyMetrics.hpp>
#include<chimbuko/core/util/error.hpp>
#include "gtest/gtest.h"
#include "../../../unit_test_common.hpp"

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

TEST(FuncAnomalyMetricsTest, TestAddAndState){
  int pid = 0;
  int rid = 1;
  int step = 2;
  int tid = 3;
  int fid = 4;
  std::string func = "myfunc";

  eventID id1(rid,step,1), id2(rid,step,2);
  
  ExecData_t 
    e1(id1, pid, rid, tid, fid, func, 100, 200),
    e2(id2, pid, rid, tid, fid, func, 300, 450);
  
  e1.set_label(-1);
  e1.set_outlier_score(3.14);
  e2.set_label(-1);
  e2.set_outlier_score(6.28);
  
  RunStats expect_sev(true); //also collect sum
  expect_sev.push(100); //use runtime
  expect_sev.push(150); 

  RunStats expect_score(true);
  expect_score.push(3.14);
  expect_score.push(6.28);
  
  FuncAnomalyMetrics met(fid,func);
  EXPECT_EQ(met.get_fid(), fid);
  EXPECT_EQ(met.get_func(), func);

  met.add(e1);
  met.add(e2);
  
  EXPECT_EQ( met.get_score(), expect_score );
  EXPECT_EQ( met.get_severity(), expect_sev );
  EXPECT_EQ( met.get_count() , 2);  
}

