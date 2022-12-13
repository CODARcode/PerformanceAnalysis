#include<chimbuko_config.h>
#include<chimbuko/util/Anomalies.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

using namespace chimbuko;


TEST(TestAnomalies, basic){
  unsigned long fid1 = 11, fid2 = 22;
  std::list<ExecData_t> execs = { createFuncExecData_t(0,1,2,fid1,"func1",0,100),  createFuncExecData_t(3,4,5,fid2,"func2",0,100),  createFuncExecData_t(5,6,8,fid2,"func2",200,300) };
  std::vector<CallListIterator_t> exec_its;
  for(auto it = execs.begin(); it != execs.end(); ++it) exec_its.push_back(it);

  { //test anomaly insert
    exec_its[0]->set_label(-1);
    exec_its[1]->set_label(-1);

    Anomalies anom;  
    EXPECT_EQ(anom.nEvents(), 0);

    anom.recordAnomaly(exec_its[0]);    
    EXPECT_EQ(anom.nEventsRecorded(Anomalies::EventType::Outlier), 1);
    EXPECT_EQ(anom.nEventsRecorded(Anomalies::EventType::Normal), 0);
    EXPECT_EQ(anom.nEvents(), 1);

    anom.recordAnomaly(exec_its[1]);    
    EXPECT_EQ(anom.nEventsRecorded(Anomalies::EventType::Outlier), 2);
    EXPECT_EQ(anom.nEventsRecorded(Anomalies::EventType::Normal), 0);
    EXPECT_EQ(anom.nEvents(), 2);

    EXPECT_EQ(anom.nFuncEventsRecorded(fid1, Anomalies::EventType::Outlier), 1);
    EXPECT_EQ(anom.nFuncEventsRecorded(fid2, Anomalies::EventType::Outlier), 1);
    EXPECT_EQ(anom.nFuncEventsRecorded(1234, Anomalies::EventType::Outlier), 0);
    
    auto v =anom.funcEventsRecorded(fid1, Anomalies::EventType::Outlier);
    ASSERT_EQ(v.size(),1);
    EXPECT_EQ(v[0], exec_its[0]);

    v =anom.funcEventsRecorded(fid2, Anomalies::EventType::Outlier);
    ASSERT_EQ(v.size(),1);
    EXPECT_EQ(v[0], exec_its[1]);
  }


  { //test normal event insert
    exec_its[0]->set_label(1);
    exec_its[1]->set_label(1);
    exec_its[2]->set_label(1); //same fid as [1]
   
    exec_its[0]->set_outlier_score(99);
    exec_its[1]->set_outlier_score(99);
    exec_its[2]->set_outlier_score(98); //lower score

    Anomalies anom;  

    bool r = anom.recordNormalEventConditional(exec_its[0]);    
    EXPECT_TRUE(r);
    EXPECT_EQ(anom.nEventsRecorded(Anomalies::EventType::Outlier), 0);
    EXPECT_EQ(anom.nEventsRecorded(Anomalies::EventType::Normal), 1);
    EXPECT_EQ(anom.nEvents(), 1);

    r = anom.recordNormalEventConditional(exec_its[1]);    //different fid so should be recorded also
    EXPECT_TRUE(r);
    EXPECT_EQ(anom.nEventsRecorded(Anomalies::EventType::Outlier), 0);
    EXPECT_EQ(anom.nEventsRecorded(Anomalies::EventType::Normal), 2);
    EXPECT_EQ(anom.nEvents(), 2);

    r = anom.recordNormalEventConditional(exec_its[2]);    //same fid, lower score so it should overwrite existing entry
    EXPECT_TRUE(r);
    EXPECT_EQ(anom.nEventsRecorded(Anomalies::EventType::Outlier), 0);
    EXPECT_EQ(anom.nEventsRecorded(Anomalies::EventType::Normal), 2);
    EXPECT_EQ(anom.nEvents(), 3); //still increment event counter

    //try inserting an event with a higher score (ok to reuse [1]), it should not be recorded
    r = anom.recordNormalEventConditional(exec_its[1]);
    EXPECT_FALSE(r);
    EXPECT_EQ(anom.nEventsRecorded(Anomalies::EventType::Outlier), 0);
    EXPECT_EQ(anom.nEventsRecorded(Anomalies::EventType::Normal), 2);
    EXPECT_EQ(anom.nEvents(), 4); //still increment event counter

    EXPECT_EQ(anom.nFuncEventsRecorded(fid1,Anomalies::EventType::Normal), 1);
    EXPECT_EQ(anom.nFuncEventsRecorded(fid2,Anomalies::EventType::Normal), 1);
    EXPECT_EQ(anom.nFuncEventsRecorded(1234,Anomalies::EventType::Normal), 0);
    
    auto v =anom.funcEventsRecorded(fid1, Anomalies::EventType::Normal);
    ASSERT_EQ(v.size(),1);
    EXPECT_EQ(v[0], exec_its[0]);

    v =anom.funcEventsRecorded(fid2, Anomalies::EventType::Normal);
    ASSERT_EQ(v.size(),1);
    EXPECT_EQ(v[0], exec_its[2]);
  }


}
