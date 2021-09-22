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

    ADLocalFuncStatistics::State fstate;
    fstate.anomaly = adata.get_state();
    fstate.func.push_back(fstats.get_state());

    lclfstats.set_state(fstate);

    RunStats cstats;
    for(int i=0;i<100;i++) cstats.push((double)i);
  
    ADLocalCounterStatistics::State cstate;
    cstate.step = step;
    cstate.counters.resize(1);
    cstate.counters[0].pid = pid;
    cstate.counters[0].name = "mycounter";
    cstate.counters[0].stats = cstats.get_state();
  
    lclcstats.set_state(cstate);

    ADLocalAnomalyMetrics::State mstate;
    mstate.app = pid;
    mstate.rank = rid;
    mstate.step = step;
    mstate.first_event_ts = 1000;
    mstate.last_event_ts = 2000;
    
    FuncAnomalyMetrics::State mfstate;
    mfstate.score = cstats.get_state();
    mfstate.severity = cstats.get_state();
    mfstate.count = 100;
    mfstate.fid = 22;
    mfstate.func = "myfunc";
    mstate.func_anom_metrics[22] = std::move(mfstate);
    
    lclmetrics.set_state(mstate);
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
  
