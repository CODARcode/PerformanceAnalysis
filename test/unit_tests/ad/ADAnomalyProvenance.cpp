#include<chimbuko/ad/ADAnomalyProvenance.hpp>
#include<chimbuko/param/sstd_param.hpp>
#include<chimbuko/util/map.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>

using namespace chimbuko;

TEST(TestADAnomalyProvenance, extractsCallInformation){
  ExecData_t exec0 = createFuncExecData_t(1,2,3, 55, "theroot", 100, 0);  //0 runtime indicates it has yet to complete
  ExecData_t exec1 = createFuncExecData_t(1,2,3, 55, "theparent", 1000, 100);
  ExecData_t exec2 = createFuncExecData_t(1,2,3, 77, "thechild", 1050, 50); //going to be the anomalous one
  exec2.set_parent(exec1.get_id());
  exec1.inc_n_children();
  exec1.update_exclusive(exec2.get_runtime());
  exec1.set_parent(exec0.get_id());
  exec0.inc_n_children();
  exec0.update_exclusive(exec1.get_runtime());

  exec2.set_label(-1); //tag it as an anomaly
  exec2.set_outlier_score(3.14);

  ADEvent event_man;
  event_man.addCall(exec0);
  event_man.addCall(exec1);
  event_man.addCall(exec2);

  RunStats stats;
  for(int i=0;i<50;i++)
    stats.push(double(i));

  SstdParam param;
  param[77] = stats;

  ADCounter counter;
  ADMetadataParser metadata;

  int step = 11;
  unsigned long step_start = 0;
  unsigned long step_end = 2000;

  ADAnomalyProvenance prov(exec2,
			   event_man,
			   param,
			   counter, metadata, 0,
			   step, step_start, step_end);

  nlohmann::json output = prov.get_json();
  //Test pid,rid,tid,runtime
  EXPECT_EQ(output["pid"], 1);
  EXPECT_EQ(output["rid"], 2);
  EXPECT_EQ(output["tid"], 3);
  EXPECT_EQ(output["runtime_total"], 50);

  //Check call stack
  EXPECT_EQ(output["call_stack"].size(), 3);
  EXPECT_EQ(output["call_stack"][0]["event_id"], exec2.get_id().toString());
  EXPECT_EQ(output["call_stack"][0]["func"], "thechild");
  EXPECT_EQ(output["call_stack"][0]["entry"], 1050);
  EXPECT_EQ(output["call_stack"][0]["exit"], 1100);
  EXPECT_EQ(output["call_stack"][0]["is_anomaly"], true);


  EXPECT_EQ(output["call_stack"][1]["event_id"], exec1.get_id().toString());
  EXPECT_EQ(output["call_stack"][1]["func"], "theparent");
  EXPECT_EQ(output["call_stack"][1]["entry"], 1000);
  EXPECT_EQ(output["call_stack"][1]["exit"], 1100);
  EXPECT_EQ(output["call_stack"][1]["is_anomaly"], false);

  EXPECT_EQ(output["call_stack"][2]["event_id"], exec0.get_id().toString());
  EXPECT_EQ(output["call_stack"][2]["func"], "theroot");
  EXPECT_EQ(output["call_stack"][2]["entry"], 100);
  EXPECT_EQ(output["call_stack"][2]["exit"], 0);
  EXPECT_EQ(output["call_stack"][2]["is_anomaly"], false);


  //Test statistics
  EXPECT_EQ(output["algo_params"], stats.get_json());

  //Check step info
  EXPECT_EQ(output["io_step"], step);
  EXPECT_EQ(output["io_step_tstart"], step_start);
  EXPECT_EQ(output["io_step_tend"], step_end);

  //Check outlier scoree
  EXPECT_EQ(output["outlier_score"], 3.14);
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
			   counter, metadata, 0,
			   11,900,1200);

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
  ExecData_t exec_gpu = createFuncExecData_t(0,1, gpu_thr, 55, "thegpufunction", 1000, 100); //on gpu
  exec_gpu.add_counter(createCounterData_t(0,1, gpu_thr, corrid_cid,  corridx, 1000, "Correlation ID"));

  ExecData_t exec_cpu = createFuncExecData_t(0,1, 0, 44, "thecpufunction", 1000, 100); //not on gpu
  exec_cpu.add_counter(createCounterData_t(0,1, 0, corrid_cid,  corridx, 1000, "Correlation ID"));

  ExecData_t exec_cpu_parent = createFuncExecData_t(0,1, 0, 11, "thecpufunctions_parent", 1000, 100); //not on gpu
  bindParentChild(exec_cpu_parent, exec_cpu);

  ADEvent event_man;
  CallListIterator_t exec_cpu_parent_it = event_man.addCall(exec_cpu_parent);
  CallListIterator_t exec_cpu_it = event_man.addCall(exec_cpu);

  //Check both CPU calls picked up
  const CallListMap_p_t &calls = event_man.getCallListMap();
  CallList_t const* calls_p_r_t_cpu_ptr = getElemPRT(0,1,0, calls);
  EXPECT_NE(calls_p_r_t_cpu_ptr, nullptr);
  const CallList_t &calls_p_r_t_cpu = *calls_p_r_t_cpu_ptr;

  EXPECT_EQ(calls_p_r_t_cpu.size(), 2);

  //Populate all the other stuff required to generate anomaly data
  RunStats stats;
  for(int i=0;i<50;i++)
    stats.push(double(i));

  SstdParam param;
  param[44] = stats;
  param[55] = stats;
  param[11] = stats;

  ADCounter counter;

  ADMetadataParser metadata;
  std::vector<MetaData_t> mdata = {
    MetaData_t(0,0, gpu_thr, "CUDA Context", "8"),
    MetaData_t(0,0, gpu_thr, "CUDA Stream", "1"),
    MetaData_t(0,0, gpu_thr, "CUDA Device", "7"),
    MetaData_t(0,0, gpu_thr, "GPU[7] Device Name", "Fake GPU")
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


  ADAnomalyProvenance prov_nongpu(*exec_cpu_it,
				  event_man,
				  param,
				  counter, metadata, 0,
				  11,900,1200);
  {
    nlohmann::json output = prov_nongpu.get_json();
    std::cout << "For CPU event, got: " << output.dump() << std::endl;
    EXPECT_EQ(output["is_gpu_event"], false);
    EXPECT_EQ(output["call_stack"].size(), 2);
  }

  //Parent function exec should not be trimmed out even though it is not the immediate partner event
  delete event_man.trimCallList();
  EXPECT_EQ(calls_p_r_t_cpu.size(), 2);

  CallListIterator_t exec_gpu_it = event_man.addCall(exec_gpu);

  ADAnomalyProvenance prov_gpu(*exec_gpu_it,
			       event_man,
			       param,
			       counter, metadata, 0,
			       11,900,1200);
  {
    nlohmann::json output = prov_gpu.get_json();
    std::cout << "For GPU event, got: " << output.dump() << std::endl;

    EXPECT_EQ(output["is_gpu_event"], true);
    EXPECT_EQ(output["gpu_location"]["context"], 8);
    EXPECT_EQ(output["gpu_location"]["device"], 7);
    EXPECT_EQ(output["gpu_location"]["stream"], 1);
  }

  delete event_man.trimCallList();
  EXPECT_EQ(calls_p_r_t_cpu.size(), 0);
}




TEST(TestADAnomalyProvenance, gracefullyFailsIfCorrelationIDissues){
  int gpu_thr = 9;
  int corrid_cid = 22; //counter index!
  
  ADEvent event_man;

  //Populate all the other stuff required to generate anomaly data
  RunStats stats;
  for(int i=0;i<50;i++)
    stats.push(double(i));

  SstdParam param;
  param[44] = stats;
  param[55] = stats;
  param[11] = stats;

  ADCounter counter;

  ADMetadataParser metadata;
  std::vector<MetaData_t> mdata = {
    MetaData_t(0,0, gpu_thr, "CUDA Context", "8"),
    MetaData_t(0,0, gpu_thr, "CUDA Stream", "1"),
    MetaData_t(0,0, gpu_thr, "CUDA Device", "7"),
    MetaData_t(0,0, gpu_thr, "GPU[7] Device Name", "Fake GPU")
  };
  metadata.addData(mdata);
  
  {
    std::cout <<  "Testing failure due to missing correlation ID" << std::endl;

    //Have a host correlation ID but not a device one
    int corridx1 = 1234;
    ExecData_t exec_gpu = createFuncExecData_t(0,1, gpu_thr, 55, "thegpufunction", 1000, 100); //on gpu
    
    ExecData_t exec_cpu = createFuncExecData_t(0,1, 0, 44, "thecpufunction", 1000, 100); //not on gpu
    exec_cpu.add_counter(createCounterData_t(0,1, 0, corrid_cid,  corridx1, 1000, "Correlation ID"));
    
    CallListIterator_t exec_cpu_it = event_man.addCall(exec_cpu);    
    CallListIterator_t exec_gpu_it = event_man.addCall(exec_gpu);
    
    ADAnomalyProvenance prov_gpu(*exec_gpu_it,
				 event_man,
				 param,
				 counter, metadata, 0,
				 11,900,1200);
    {
      nlohmann::json output = prov_gpu.get_json();
      std::cout << "For GPU event, got: " << output.dump() << std::endl;
    
      EXPECT_EQ(output["is_gpu_event"], true);
      EXPECT_EQ(output["gpu_location"]["context"], 8);
      EXPECT_EQ(output["gpu_location"]["device"], 7);
      EXPECT_EQ(output["gpu_location"]["stream"], 1);

      std::string got = output["gpu_parent"];
      std::string expect = "Chimbuko error: Correlation ID of host parent event was not recorded";
      std::cout << got << std::endl;

      EXPECT_EQ(got, expect);
    }
  }

  //Failure due to multiple correlation IDs
  {
    std::cout <<  "Testing failure due to multiple correlation IDs" << std::endl;
    int corridx2 = 2222, corridx3 = 3333;

    ExecData_t exec_gpu = createFuncExecData_t(0,1, gpu_thr, 55, "thegpufunction", 1000, 100); //on gpu
    exec_gpu.add_counter(createCounterData_t(0,1, gpu_thr, corrid_cid,  corridx2, 1000, "Correlation ID")); //this one has 2 correlation IDs
    exec_gpu.add_counter(createCounterData_t(0,1, gpu_thr, corrid_cid,  corridx3, 1000, "Correlation ID"));
    
    ExecData_t exec_cpu = createFuncExecData_t(0,1, 0, 44, "thecpufunction", 1000, 100); //not on gpu
    exec_cpu.add_counter(createCounterData_t(0,1, 0, corrid_cid,  corridx2, 1000, "Correlation ID"));

    ExecData_t exec_cpu2 = createFuncExecData_t(0,1, 0, 66, "theothercpufunction", 1000, 100); //not on gpu
    exec_cpu2.add_counter(createCounterData_t(0,1, 0, corrid_cid,  corridx3, 1000, "Correlation ID"));

    CallListIterator_t exec_cpu_it = event_man.addCall(exec_cpu);
    CallListIterator_t exec_cpu2_it = event_man.addCall(exec_cpu2);
    
    CallListIterator_t exec_gpu_it = event_man.addCall(exec_gpu);

    ADAnomalyProvenance prov_gpu(*exec_gpu_it,
				 event_man,
				 param,
				 counter, metadata, 0,
				 11,900,1200);
    {
      nlohmann::json output = prov_gpu.get_json();
      std::cout << "For GPU event, got: " << output.dump() << std::endl;

      EXPECT_EQ(output["is_gpu_event"], true);
      EXPECT_EQ(output["gpu_location"]["context"], 8);
      EXPECT_EQ(output["gpu_location"]["device"], 7);
      EXPECT_EQ(output["gpu_location"]["stream"], 1);

      std::string got = output["gpu_parent"];
      std::string expect = "Chimbuko error: Multiple host parent event correlation IDs found, likely due to trace corruption";
      std::cout << got << std::endl;

      EXPECT_EQ(got, expect);
    }
  }

  {
    std::cout <<  "Testing failure due to missing parent event" << std::endl;

    //Have a host correlation ID but not a device one
    int corridx4 = 4444;
    ExecData_t exec_gpu = createFuncExecData_t(0,1, gpu_thr, 55, "thegpufunction", 1000, 100); //on gpu
    exec_gpu.add_counter(createCounterData_t(0,1, gpu_thr, corrid_cid,  corridx4, 1000, "Correlation ID")); //this one has 2 correlation IDs
    
    ExecData_t exec_cpu = createFuncExecData_t(0,1, 0, 44, "thecpufunction", 1000, 100); //not on gpu
    exec_cpu.add_counter(createCounterData_t(0,1, 0, corrid_cid,  corridx4, 1000, "Correlation ID"));
    
    CallListIterator_t exec_cpu_it = event_man.addCall(exec_cpu);

    //Force the trimming out of the cpu event
    exec_cpu_it->can_delete(true);
    delete event_man.trimCallList();

    CallListIterator_t exec_gpu_it = event_man.addCall(exec_gpu);
    
    ADAnomalyProvenance prov_gpu(*exec_gpu_it,
				 event_man,
				 param,
				 counter, metadata, 0,
				 11,900,1200);
    {
      nlohmann::json output = prov_gpu.get_json();
      std::cout << "For GPU event, got: " << output.dump() << std::endl;
    
      EXPECT_EQ(output["is_gpu_event"], true);
      EXPECT_EQ(output["gpu_location"]["context"], 8);
      EXPECT_EQ(output["gpu_location"]["device"], 7);
      EXPECT_EQ(output["gpu_location"]["stream"], 1);

      std::string got = output["gpu_parent"];
      std::string expect = "Chimbuko error: Host parent event could not be reached";
      std::cout << got << std::endl;

      EXPECT_EQ(got, expect);
    }
  }


  


}






TEST(TestADAnomalyProvenance, extractsExecWindow){
  ExecData_t exec0 = createFuncExecData_t(1,2,3, 33, "theonebefore", 900, 0); //not yet completed
  ExecData_t exec1 = createFuncExecData_t(1,2,3, 55, "theparent", 1000, 100);
  ExecData_t exec2 = createFuncExecData_t(1,2,3, 77, "thechild", 1050, 50); //going to be the anomalous one
  ExecData_t exec3 = createFuncExecData_t(1,2,3, 88, "theoneafter", 1100, 100);
  exec2.set_parent(exec1.get_id());
  exec1.inc_n_children();
  exec1.update_exclusive(exec2.get_runtime());

  exec2.set_label(-1);

  ExecData_t* execs[] = {&exec0, &exec1, &exec2, &exec3};

  //Add some comms
  CommData_t comm1 = createCommData_t(1,2,3, 0, 111, 2, 1024, 1002, "SEND");
  CommData_t comm2 = createCommData_t(1,2,3, 1, 112, 2, 1024, 1133, "RECV");

  CommData_t* comms[] = {&comm1, &comm2};

  ASSERT_NE( exec1.add_message(comm1), false );
  ASSERT_NE( exec3.add_message(comm2), false );

  ADEvent event_man;
  event_man.addCall(exec0);
  event_man.addCall(exec1);
  event_man.addCall(exec2);
  event_man.addCall(exec3);

  RunStats stats; //doesn't matter
  for(int i=0;i<50;i++)
    stats.push(double(i));

  SstdParam param;
  param[77] = stats;

  ADCounter counter;
  ADMetadataParser metadata;

  ADAnomalyProvenance prov(exec2,
			   event_man,
			   param,
			   counter, metadata,
			   2,
			   8,800,1200);

  nlohmann::json output = prov.get_json();

  const nlohmann::json &win = output["event_window"];
  const nlohmann::json &ewin = win["exec_window"];
  const nlohmann::json &cwin = win["comm_window"];
  EXPECT_EQ(ewin.size(), 4);
  EXPECT_EQ(cwin.size(), 2);
  for(int i=0;i<4;i++){
    EXPECT_EQ(ewin[i]["event_id"], execs[i]->get_id().toString());
    EXPECT_EQ(ewin[i]["entry"], execs[i]->get_entry());
    EXPECT_EQ(ewin[i]["exit"], execs[i]->get_exit());
    EXPECT_EQ(ewin[i]["is_anomaly"], execs[i]->get_label() == -1);
    EXPECT_EQ(ewin[i]["parent_event_id"], execs[i]->get_parent().toString());
  }

  for(int i=0;i<2;i++)
    EXPECT_EQ(cwin[i]["tag"], comms[i]->tag());
}
