#include<chimbuko/ad/ADLocalAnomalyMetrics.hpp>
#include<chimbuko/util/error.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"


using namespace chimbuko;


TEST(ADLocalAnomalyMetricsTest, TestGenerateAndState){
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
   
  RunStats expect_sev(true); //do accumulate
  expect_sev.push(100); //use runtime
  expect_sev.push(150); 

  RunStats expect_score(true);
  expect_score.push(3.14);
  expect_score.push(6.28);
  
  std::list<ExecData_t> calllist;
  calllist.push_back(e1);
  calllist.push_back(e2);
  
  ExecDataMap_t exec_data;
  exec_data[fid].push_back(calllist.begin());
  exec_data[fid].push_back(std::next(calllist.begin()));

  ADExecDataInterface iface(&exec_data);
  {
    auto events = iface.getDataSet(0);
    events[0].label = events[1].label = ADDataInterface::EventType::Outlier;
    events[0].score = 3.14;
    events[1].score = 6.28;
    iface.recordDataSetLabels(events,0);
  }
    
  unsigned long first_ts = 100, last_ts = 999;
  
  ADLocalAnomalyMetrics met(pid,rid,step,first_ts,last_ts,iface);

  EXPECT_EQ(met.get_pid(), pid);
  EXPECT_EQ(met.get_rid(), rid);
  EXPECT_EQ(met.get_step(), step);
  EXPECT_EQ(met.get_first_event_ts(), first_ts);
  EXPECT_EQ(met.get_last_event_ts(), last_ts);

  EXPECT_EQ(met.get_metrics().size(),1);
  EXPECT_EQ(met.get_metrics().begin()->first,fid);
  
  
  EXPECT_EQ( met.get_metrics().begin()->second.get_score().get_json().dump(4), expect_score.get_json().dump(4) );
  EXPECT_EQ( met.get_metrics().begin()->second.get_severity().get_json().dump(4), expect_sev.get_json().dump(4) );
  EXPECT_EQ( met.get_metrics().begin()->second.get_count() , 2);  
}

