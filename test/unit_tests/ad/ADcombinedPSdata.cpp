#include<chimbuko/ad/ADcombinedPSdata.hpp>
#include<chimbuko/pserver/NetPayloadRecvCombinedADdata.hpp>
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
  
