#include<chimbuko/ad/ADAnomalyProvenance.hpp>
#include<chimbuko/param/sstd_param.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>

using namespace chimbuko;

TEST(TestADAnomalyProvenance, extractsCallInformation){
  ExecData_t exec1 = createFuncExecData_t(1,2,3, 55, "theparent", 1000, 100);
  ExecData_t exec2 = createFuncExecData_t(1,2,3, 77, "thechild", 1050, 50); //going to be the anomalous one
  exec2.set_parent(exec1.get_id());
  exec1.inc_n_children();
  exec1.update_exclusive(exec2.get_runtime());

  ADEvent event_man;
  event_man.addCall(exec1);
  event_man.addCall(exec2);

  RunStats stats;
  for(int i=0;i<50;i++)
    stats.push(double(i));

  SstdParam param;
  param[77] = stats;

  ADCounter counter;
  ADMetadataParser metadata;

  ADAnomalyProvenance prov(exec2,
			   event_man,
			   param,
			   counter, metadata);
    
  nlohmann::json output = prov.get_json();
  //Test pid,rid,tid,runtime
  EXPECT_EQ(output["pid"], 1);
  EXPECT_EQ(output["rid"], 2);
  EXPECT_EQ(output["tid"], 3);
  EXPECT_EQ(output["runtime_total"], 50);
  
  //Check call stack
  EXPECT_EQ(output["call_stack"].size(), 2);
  EXPECT_EQ(output["call_stack"][0]["func"], "thechild");
  EXPECT_EQ(output["call_stack"][0]["entry_time"], 1050);
  EXPECT_EQ(output["call_stack"][1]["func"], "theparent");
  EXPECT_EQ(output["call_stack"][1]["entry_time"], 1000);
  //Test statistics
  EXPECT_EQ(output["func_stats"], stats.get_json());
  
}



TEST(TestADAnomalyProvenance, findsAssociatedCounters){
  ExecData_t exec1 = createFuncExecData_t(0,1,0, 55, "theparent", 1000, 100);
  ExecData_t exec2 = createFuncExecData_t(0,1,0, 77, "thechild", 1050, 50); //going to be the anomalous one
  exec2.set_parent(exec1.get_id());
  exec1.inc_n_children();
  exec1.update_exclusive(exec2.get_runtime());

  ADEvent event_man;
  event_man.addCall(exec1);
  event_man.addCall(exec2);

  RunStats stats;
  for(int i=0;i<50;i++)
    stats.push(double(i));

  SstdParam param;
  param[77] = stats;

  std::unordered_map<int, std::string> counters = {  {44, "counter1"}, {55, "counter2"} };

  std::vector<Event_t> cevents = {
    createCounterEvent_t(0,1, 0, 44, 1777,   1020), //before anomalous function, shouldn't be picked up
    createCounterEvent_t(0,1, 0, 55, 1778,   1050), //at start of anomalous function, should be picked up
    createCounterEvent_t(0,1, 1, 55, 1779,   1070), //during anomalous function but on different thread, shouldn't be picked up
    createCounterEvent_t(0,1, 0, 44, 1780,   1075), //during anomalous function, should be picked up
    createCounterEvent_t(0,1, 0, 55, 1781,   1100), //at end of anomalous function, should be picked up
    createCounterEvent_t(0,1, 0, 55, 1781,   1100), //at end of anomalous function, should be picked up
    createCounterEvent_t(0,1, 0, 44, 1782,   1101) //after anomalous function, shouldn't be picked up
  };
  int idx_pickedup[] = {1,3,4,5};

  ADCounter counter;
  counter.linkCounterMap(&counters);
  for(int i=0;i<7;i++) counter.addCounter(cevents[i]);

  //These are normally assigned by ADEvent; correct determination of counters in window tested by ADEvent unit test
  std::list<CounterDataListIterator_t> exec2_counters = counter.getCountersInWindow(0,1,0, 1050,1100);
  for(auto const &e : exec2_counters)
    exec2.add_counter(*e);
  
  ADMetadataParser metadata;

  ADAnomalyProvenance prov(exec2,
			   event_man,
			   param,
			   counter, metadata);
    
  nlohmann::json output = prov.get_json();
  

  
  EXPECT_EQ(output["counter_events"].size(), 4); 
  for(int i=0;i<4;i++){
    int cidx = idx_pickedup[i];
    std::string cname = counters[cevents[cidx].counter_id()];
    CounterData_t cd(cevents[cidx], cname);
    EXPECT_EQ(output["counter_events"][i], cd.get_json() );
  }
}


TEST(TestADAnomalyProvenance, detectsGPUevents){
  int gpu_thr = 9;
  int corridx = 1234;
  int corrid_cid = 22;
  ExecData_t exec1 = createFuncExecData_t(0,1, gpu_thr, 55, "thegpufunction", 1000, 100); //on gpu
  exec1.add_counter(createCounterData_t(0,1, gpu_thr, corrid_cid,  corridx, 1000, "Correlation ID"));
  
  ExecData_t exec2 = createFuncExecData_t(0,1, 0, 44, "thecpufunction", 1000, 100); //not on gpu
  exec2.add_counter(createCounterData_t(0,1, 0, corrid_cid,  corridx, 1000, "Correlation ID"));

  ADEvent event_man;
  CallListIterator_t exec1_it = event_man.addCall(exec1);
  CallListIterator_t exec2_it = event_man.addCall(exec2);

  RunStats stats;
  for(int i=0;i<50;i++)
    stats.push(double(i));

  SstdParam param;
  param[44] = stats;
  param[55] = stats;

  ADCounter counter;
    
  ADMetadataParser metadata;
  std::vector<MetaData_t> mdata = {
    MetaData_t(0, gpu_thr, "CUDA Context", "8"),
    MetaData_t(0, gpu_thr, "CUDA Stream", "1"),
    MetaData_t(0, gpu_thr, "CUDA Device", "7"),
    MetaData_t(0, gpu_thr, "GPU[7] Device Name", "Fake GPU")
  };
  metadata.addData(mdata);
  EXPECT_EQ( metadata.isGPUthread(gpu_thr), true );
  EXPECT_EQ( metadata.isGPUthread(0), false );
  EXPECT_EQ( metadata.getGPUthreadInfo(gpu_thr).context, 8);
  EXPECT_EQ( metadata.getGPUthreadInfo(gpu_thr).device, 7);
  EXPECT_EQ( metadata.getGPUthreadInfo(gpu_thr).stream, 1);
  auto it = metadata.getGPUproperties().find(7);
  EXPECT_NE( it, metadata.getGPUproperties().end() );
  auto pit = it->second.find("Device Name");
  EXPECT_NE( pit, it->second.end() );
  EXPECT_EQ( pit->second, "Fake GPU" );

  ADAnomalyProvenance prov_gpu(*exec1_it,
			       event_man,
			       param,
			       counter, metadata);
    
  nlohmann::json output = prov_gpu.get_json();
  std::cout << "For GPU event, got: " << output.dump() << std::endl;

  EXPECT_EQ(output["is_gpu_event"], true);
  EXPECT_EQ(output["gpu_location"]["context"], 8);
  EXPECT_EQ(output["gpu_location"]["device"], 7);
  EXPECT_EQ(output["gpu_location"]["stream"], 1);

  ADAnomalyProvenance prov_nongpu(*exec2_it,
			       event_man,
			       param,
			       counter, metadata);
  output = prov_nongpu.get_json();
  std::cout << "For CPU event, got: " << output.dump() << std::endl;
  EXPECT_EQ(output["is_gpu_event"], false);
}
