#include <random>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include <chimbuko/pserver/GlobalAnomalyMetrics.hpp>

using namespace chimbuko;

TEST(GlobalAnomalyMetricsTest, TestAggregation){  
  int pid = 0;
  int rid = 1;
  int step1= 2;
  int step2= 3;
  int tid = 3;
  int fid = 4;
  std::string func = "myfunc";

  LocalNet net;
  ADLocalNetClient net_client;
  net_client.connect_ps(rid);

  GlobalAnomalyMetrics glb, glb_net; //test net send route
  net.add_payload(new NetPayloadUpdateAnomalyMetrics(&glb_net));

  //Add first step
  {
    eventID id1(rid,step1,1), id2(rid,step1,2);
    
    ExecData_t 
      e1(id1, pid, rid, tid, fid, func, 100, 200),
      e2(id2, pid, rid, tid, fid, func, 300, 450);
    
    e1.set_label(-1);
    e1.set_outlier_score(3.14);
    e2.set_label(-1);
    e2.set_outlier_score(6.28);
    
    std::list<ExecData_t> calllist;
    calllist.push_back(e1);
    calllist.push_back(e2);
    
    Anomalies anom;
    anom.insert(calllist.begin(),Anomalies::EventType::Outlier);
    anom.insert(std::next(calllist.begin(),1),Anomalies::EventType::Outlier);
    
    unsigned long first_ts = 100, last_ts = 999;
    
    ADLocalAnomalyMetrics lcl(pid,rid,step1,first_ts,last_ts,anom);
    
    glb.add(lcl);
    lcl.send(net_client); //also send by net
  }  

  auto rec_pit = glb.getRecentMetrics().find(pid);
  ASSERT_NE(rec_pit, glb.getToDateMetrics().end());
  
  auto rec_rit = rec_pit->second.find(rid);
  ASSERT_NE(rec_rit, rec_pit->second.end());

  auto rec_fit = rec_rit->second.find(fid);
  ASSERT_NE(rec_fit, rec_rit->second.end());

  EXPECT_EQ( rec_fit->second.get_score_stats().count(), 2);
  EXPECT_EQ( rec_fit->second.get_first_io_step(), step1 );
  EXPECT_EQ( rec_fit->second.get_last_io_step(), step1 );


  auto run_pit = glb.getToDateMetrics().find(pid);
  ASSERT_NE(run_pit, glb.getToDateMetrics().end());
  
  auto run_rit = run_pit->second.find(rid);
  ASSERT_NE(run_rit, run_pit->second.end());

  auto run_fit = run_rit->second.find(fid);
  ASSERT_NE(run_fit, run_rit->second.end());

  EXPECT_EQ( run_fit->second.get_score_stats().count(), 2);
  EXPECT_EQ( run_fit->second.get_first_io_step(), step1 );
  EXPECT_EQ( run_fit->second.get_last_io_step(), step1 );

  //Test the net-sent version agrees
  EXPECT_EQ(glb, glb_net);

  //Get json and flush
  nlohmann::json json1 = glb.get_json();
  nlohmann::json json1_net = glb_net.get_json();
  EXPECT_EQ(json1, json1_net);

  //Check flush
  EXPECT_EQ( glb.getRecentMetrics().size(), 0);
  EXPECT_NE( glb.getToDateMetrics().size(), 0);


  //Add second step
  {
    eventID id1(rid,step2,1), id2(rid,step2,2);
    
    ExecData_t 
      e1(id1, pid, rid, tid, fid, func, 500, 200),
      e2(id2, pid, rid, tid, fid, func, 900, 450);
    
    e1.set_label(-1);
    e1.set_outlier_score(8.88);
    e2.set_label(-1);
    e2.set_outlier_score(21.71);
    
    std::list<ExecData_t> calllist;
    calllist.push_back(e1);
    calllist.push_back(e2);
    
    Anomalies anom;
    anom.insert(calllist.begin(),Anomalies::EventType::Outlier);
    anom.insert(std::next(calllist.begin(),1),Anomalies::EventType::Outlier);
    
    unsigned long first_ts = 1000, last_ts = 1999;
    
    ADLocalAnomalyMetrics lcl(pid,rid,step2,first_ts,last_ts,anom);
    
    glb.add(lcl);
    lcl.send(net_client);
  }  

  //Recent statistics should just be the new data
  rec_pit = glb.getRecentMetrics().find(pid);
  ASSERT_NE(rec_pit, glb.getToDateMetrics().end());
  
  rec_rit = rec_pit->second.find(rid);
  ASSERT_NE(rec_rit, rec_pit->second.end());

  rec_fit = rec_rit->second.find(fid);
  ASSERT_NE(rec_fit, rec_rit->second.end());

  EXPECT_EQ( rec_fit->second.get_score_stats().count(), 2);
  EXPECT_EQ( rec_fit->second.get_first_io_step(), step2 );
  EXPECT_EQ( rec_fit->second.get_last_io_step(), step2 );

  //To-date statistics should be all data
  EXPECT_EQ( run_fit->second.get_score_stats().count(), 4);
  EXPECT_EQ( run_fit->second.get_first_io_step(), step1 );
  EXPECT_EQ( run_fit->second.get_last_io_step(), step2 );

  //Test the net-sent version agrees
  EXPECT_EQ(glb, glb_net);
}
