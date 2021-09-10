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
  ADcombinedPSdata comb_send;

  TestSetup(): pid(0),rid(1),step(100),
	       lclfstats(pid,rid,step),
	       lclcstats(pid,step,nullptr),
	       comb_send(lclfstats, lclcstats){

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
  }

};


TEST(ADcombinedPSdataTest, Serialization){
  TestSetup setup;

  //Generate serialized string
  std::string msg = setup.comb_send.net_serialize();

  //Unserialize into target objects
  ADLocalFuncStatistics rcvfstats(0,0,0);
  ADLocalCounterStatistics rcvcstats(0,0,nullptr);
  
  ADcombinedPSdata comb_recv(rcvfstats, rcvcstats);
  comb_recv.net_deserialize(msg);

  EXPECT_EQ(rcvfstats, setup.lclfstats);
  EXPECT_EQ(rcvcstats, setup.lclcstats);
}

TEST(ADcombinedPSdataTest, NetSendRecv){
  TestSetup setup;

  LocalNet net;

  GlobalAnomalyStats astats;
  GlobalCounterStats cstats;

  net.add_payload(new NetPayloadRecvCombinedADdata(&astats, &cstats));

  ADLocalNetClient net_client;
  net_client.connect_ps(0,0,"");
  
  setup.comb_send.send(net_client);

  net_client.disconnect_ps();

  
  GlobalAnomalyStats astats_expect;
  astats_expect.add_anomaly_data(setup.lclfstats);
  
  GlobalCounterStats cstats_expect;
  cstats_expect.add_counter_data(setup.lclcstats);
  
  EXPECT_EQ(astats, astats_expect);
  EXPECT_EQ(cstats, cstats_expect);
}
  
