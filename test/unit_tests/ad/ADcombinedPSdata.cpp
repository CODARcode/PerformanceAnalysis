#include<chimbuko/modules/performance_analysis/ad/ADcombinedPSdata.hpp>
#include<chimbuko/modules/performance_analysis/pserver/NetPayloadRecvCombinedADdata.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

using namespace chimbuko;

struct TestSetup{
  int pid;
  int rid;
  int step;

  ADLocalFuncStatistics lclfstats;
  ADLocalCounterStatistics lclcstats;
  ADLocalAnomalyMetrics lclmetrics;
  ADcombinedPSdata comb_send;

  TestSetup(): pid(0),rid(1),step(100),
	       lclfstats(pid,rid,step),
	       lclcstats(pid,step,nullptr),
	       comb_send(lclfstats, lclcstats, lclmetrics){

    AnomalyData adata(pid,rid,step,0,100,55);
    FuncStats fstats(pid, 22, "myfunc");
    lclfstats.setAnomalyData(adata);
    lclfstats.setFuncStats(std::unordered_map<unsigned long, FuncStats>({{22,fstats}}));

    RunStats cstats;
    for(int i=0;i<100;i++) cstats.push((double)i);
  
    lclcstats.setProgramIndex(pid);
    lclcstats.setIOstep(step);
    lclcstats.setStats("mycounter", cstats);

    lclmetrics.set_pid(pid);
    lclmetrics.set_rid(rid);
    lclmetrics.set_step(step);
    lclmetrics.set_first_event_ts(1000);
    lclmetrics.set_last_event_ts(2000);

    FuncAnomalyMetrics mf;
    mf.set_score(cstats);
    mf.set_severity(cstats);
    mf.set_count(100);
    mf.set_fid(22);
    mf.set_func("myfunc");
    
    lclmetrics.set_metrics(std::unordered_map<int, FuncAnomalyMetrics>({{22,mf}}));
  }

};


TEST(ADcombinedPSdataTest, Serialization){
  TestSetup setup;

  //Generate serialized string
  std::string msg = setup.comb_send.net_serialize();

  //Unserialize into target objects
  ADLocalFuncStatistics rcvfstats(0,0,0);
  ADLocalCounterStatistics rcvcstats(0,0,nullptr);
  ADLocalAnomalyMetrics rcvmetrics;

  ADcombinedPSdata comb_recv(rcvfstats, rcvcstats, rcvmetrics);
  comb_recv.net_deserialize(msg);

  EXPECT_EQ(rcvfstats, setup.lclfstats);
  EXPECT_EQ(rcvcstats, setup.lclcstats);
  EXPECT_EQ(rcvmetrics, setup.lclmetrics);
}

TEST(ADcombinedPSdataTest, NetSendRecv){
  TestSetup setup;

  LocalNet net;

  GlobalAnomalyStats astats;
  GlobalCounterStats cstats;
  GlobalAnomalyMetrics mstats;

  net.add_payload(new NetPayloadRecvCombinedADdata(&astats, &cstats, &mstats));

  ADLocalNetClient net_client;
  net_client.connect_ps(0,0,"");
  
  setup.comb_send.send(net_client);

  net_client.disconnect_ps();

  
  GlobalAnomalyStats astats_expect;
  astats_expect.add_anomaly_data(setup.lclfstats);
  
  GlobalCounterStats cstats_expect;
  cstats_expect.add_counter_data(setup.lclcstats);
  
  GlobalAnomalyMetrics mstats_expect;
  mstats_expect.add(setup.lclmetrics);

  EXPECT_EQ(astats, astats_expect);
  EXPECT_EQ(cstats, cstats_expect);
  EXPECT_EQ(mstats, mstats_expect);
}




struct TestArraySetup{
  int pid;
  int rid;
  std::vector<int> step;

  std::vector<ADLocalFuncStatistics> lclfstats;
  std::vector<ADLocalCounterStatistics> lclcstats;
  std::vector<ADLocalAnomalyMetrics> lclmetrics;
  ADcombinedPSdataArray comb_send;

  TestArraySetup(int N): pid(0),rid(1),step(N),
			 lclfstats(N, ADLocalFuncStatistics(pid,rid,0)),
			 lclcstats(N, ADLocalCounterStatistics(pid,0,nullptr)),
			 lclmetrics(N),
			 comb_send(lclfstats, lclcstats, lclmetrics){
    for(int i=0;i<N;i++){
      step[i] = N-i-1; //backwards so we can check that it still takes the largest step when doing send
    
      AnomalyData adata(pid,rid,step[i],0,100*i+4,55*i+4);
      FuncStats fstats(pid, 22*i+1, "myfunc"+std::to_string(i));
      lclfstats[i].setAnomalyData(adata);
      lclfstats[i].setFuncStats(std::unordered_map<unsigned long, FuncStats>({{22*i+1,fstats}}));

      RunStats cstats;
      for(int j=0;j<100;j++) cstats.push((double)(i*33+88));
  
      lclcstats[i].setProgramIndex(pid);
      lclcstats[i].setIOstep(step[i]);
      lclcstats[i].setStats("mycounter"+std::to_string(i), cstats);      

      lclmetrics[i].set_pid(pid);
      lclmetrics[i].set_rid(rid);
      lclmetrics[i].set_step(step[i]);
      lclmetrics[i].set_first_event_ts(1000*i+10);
      lclmetrics[i].set_last_event_ts(2000*i+10);
      
      FuncAnomalyMetrics mf;
      mf.set_score(cstats);
      mf.set_severity(cstats);
      mf.set_count(100*i+99);
      mf.set_fid(22*i+1);
      mf.set_func("myfunc");
      
      lclmetrics[i].set_metrics(std::unordered_map<int, FuncAnomalyMetrics>({{22*i+1,mf}}));
    }
  }

};


TEST(ADcombinedPSdataArrayTest, Serialization){
  TestArraySetup setup(3);

  //Generate serialized string
  std::string msg = setup.comb_send.net_serialize();

  //Unserialize into target objects
  std::vector<ADLocalFuncStatistics> rcvfstats;
  std::vector<ADLocalCounterStatistics> rcvcstats;
  std::vector<ADLocalAnomalyMetrics> rcvmetrics;

  ADcombinedPSdataArray comb_recv(rcvfstats, rcvcstats, rcvmetrics);
  comb_recv.net_deserialize(msg);

  EXPECT_EQ(rcvfstats, setup.lclfstats);
  EXPECT_EQ(rcvcstats, setup.lclcstats);
  EXPECT_EQ(rcvmetrics, setup.lclmetrics);
}

TEST(ADcombinedPSdataArrayTest, NetSendRecv){
  TestArraySetup setup(3);

  LocalNet net;

  GlobalAnomalyStats astats;
  GlobalCounterStats cstats;
  GlobalAnomalyMetrics mstats;

  net.add_payload(new NetPayloadRecvCombinedADdataArray(&astats, &cstats, &mstats));

  ADLocalNetClient net_client;
  net_client.connect_ps(0,0,"");
  
  setup.comb_send.send(net_client);

  net_client.disconnect_ps();

  
  GlobalAnomalyStats astats_expect;
  GlobalCounterStats cstats_expect;
  GlobalAnomalyMetrics mstats_expect;

  for(int i=0;i<setup.lclfstats.size();i++){
    astats_expect.add_anomaly_data(setup.lclfstats[i]);
    cstats_expect.add_counter_data(setup.lclcstats[i]);
    mstats_expect.add(setup.lclmetrics[i]);
  }

  EXPECT_EQ(astats, astats_expect);
  EXPECT_EQ(cstats, cstats_expect);
  EXPECT_EQ(mstats, mstats_expect);
}
  
