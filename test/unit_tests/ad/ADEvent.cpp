#include<chimbuko/modules/performance_analysis/ad/ADEvent.hpp>
#include<chimbuko/core/util/map.hpp>
#include<chimbuko/core/util/error.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include<array>
#include<vector>

using namespace chimbuko;


//Unit testing for ADEvent::addFunc
struct FuncEvent{
  std::array<unsigned long,FUNC_EVENT_DIM> data;
  Event_t func_event;
  std::string event_type;
  std::string func_name;

  FuncEvent(const std::string &event_type, const std::string &func_name,
	    const int event_id = 1234, const int func_id = 9876, const long timestamp = 0): event_type(event_type), func_name(func_name),
											    func_event(data.data(), EventDataType::FUNC, 0){
    data[IDX_P] = 0; //program
    data[IDX_R] = 0; //rank
    data[IDX_T] = 0; //thread
    data[IDX_E] = event_id; //event
    data[FUNC_IDX_F] = func_id; //function idx
    data[FUNC_IDX_TS]  = timestamp;
  }
  FuncEvent(const FuncEvent &r): event_type(r.event_type), func_name(r.func_name), data(r.data), func_event(data.data(), EventDataType::FUNC, 0){}
};

struct ADFuncEventContainer{
  std::vector<FuncEvent> events;
  std::unordered_map<int, std::string> event_types;
  std::unordered_map<int, std::string> func_map;
  ADEvent event_manager;

  int addEvent(const FuncEvent &ev){ events.push_back(ev); return events.size() - 1; } //returns index of last added event

  void registerEvents(){
    for(int i=0;i<events.size();i++){
      int eid = events[i].data[IDX_E];
      if(event_types.count(eid) && event_types[eid] != events[i].event_type)
	throw("registerEvents: multiple event types have same eid!");
      event_types[eid] = events[i].event_type;
    }
  }
  void registerFuncs(){
    for(int i=0;i<events.size();i++){
      int fid = events[i].data[FUNC_IDX_F];
      if(func_map.count(fid) && func_map[fid] != events[i].func_name)
	throw("registerFuncs: multiple funcs have same fid!");
      func_map[fid] = events[i].func_name;
    }
  }
  void linkEventType(){
    event_manager.linkEventType(&event_types);
  }
  void linkFuncMap(){
    event_manager.linkFuncMap(&func_map);
  }
  void initializeAD(){
    registerEvents();
    registerFuncs();
    linkEventType();
    linkFuncMap();
  }
  const Event_t & operator[](const int i) const{ return events[i].func_event; };
};

TEST(ADEventTestaddFunc, eventTypesNotAssigned){
  ADFuncEventContainer ad;
  int idx = ad.addEvent(FuncEvent("MYEVENT","MYFUNC"));
  ad.registerEvents();
  ad.registerFuncs();
  //ad.linkEventType();
  ad.linkFuncMap();

  EXPECT_EQ( ad.event_manager.addFunc(ad[idx]), EventError::UnknownEvent );
}
TEST(ADEventTestaddFunc, eventNotInMap){
  ADFuncEventContainer ad;
  int idx = ad.addEvent(FuncEvent("MYEVENT","MYFUNC"));
  //ad.registerEvents();
  ad.registerFuncs();
  ad.linkEventType();
  ad.linkFuncMap();

  EXPECT_EQ( ad.event_manager.addFunc(ad[idx]), EventError::UnknownEvent );
}
TEST(ADEventTestaddFunc, funcMapNotAssigned){
  ADFuncEventContainer ad;
  int idx = ad.addEvent(FuncEvent("MYEVENT","MYFUNC"));
  ad.registerEvents();
  ad.registerFuncs();
  ad.linkEventType();
  //ad.linkFuncMap();

  EXPECT_EQ( ad.event_manager.addFunc(ad[idx]), EventError::UnknownEvent );
}
TEST(ADEventTestaddFunc, funcNotInMap){
  ADFuncEventContainer ad;
  int idx = ad.addEvent(FuncEvent("MYEVENT","MYFUNC"));
  ad.registerEvents();
  //ad.registerFuncs();
  ad.linkEventType();
  ad.linkFuncMap();

  EXPECT_EQ( ad.event_manager.addFunc(ad[idx]), EventError::UnknownFunc );
}
TEST(ADEventTestaddFunc, eventNotENTRYorEXIT){
  ADFuncEventContainer ad;
  int idx = ad.addEvent(FuncEvent("INVALID_EVENT","MYFUNC"));
  ad.initializeAD();
  EXPECT_EQ( ad.event_manager.addFunc(ad[idx]), EventError::UnknownEvent );
}
TEST(ADEventTestaddFunc, eventisENTRY){
  ADFuncEventContainer ad;
  int idx = ad.addEvent(FuncEvent("ENTRY","MYFUNC"));
  ad.initializeAD();
  EXPECT_EQ( ad.event_manager.addFunc(ad[idx]), EventError::OK);
}
TEST(ADEventTestaddFunc, eventisEXITwithoutENTRY){
  ADFuncEventContainer ad;
  int idx = ad.addEvent(FuncEvent("EXIT","MYFUNC"));
  ad.initializeAD();
  EXPECT_EQ( ad.event_manager.addFunc(ad[idx]), EventError::EmptyCallStack );
}
TEST(ADEventTestaddFunc, eventisEXITwithENTRYbutDifferentFuncID){
  ADFuncEventContainer ad;
  int idx_entry = ad.addEvent(FuncEvent("ENTRY","MYFUNC",1,1));
  int idx_exit = ad.addEvent(FuncEvent("EXIT","MYFUNC",2,2));
  ad.initializeAD();
  EXPECT_EQ( ad.event_manager.addFunc(ad[idx_entry]), EventError::OK );
  EXPECT_EQ( ad.event_manager.addFunc(ad[idx_exit]), EventError::CallStackViolation );
}
TEST(ADEventTestaddFunc, eventisEXITwithENTRY){
  ADFuncEventContainer ad;
  int idx_entry = ad.addEvent(FuncEvent("ENTRY","MYFUNC",1,1));
  int idx_exit = ad.addEvent(FuncEvent("EXIT","MYFUNC",2,1));
  ad.initializeAD();
  EXPECT_EQ( ad.event_manager.addFunc(ad[idx_entry]), EventError::OK );
  EXPECT_EQ( ad.event_manager.addFunc(ad[idx_exit]), EventError::OK );
}







//Unit testing for ADEvent::addComm
struct CommEvent{
  std::array<unsigned long,COMM_EVENT_DIM> data;
  Event_t comm_event;
  std::string event_type;

  CommEvent(const std::string &event_type,
	    const int event_id = 1234, const long timestamp = 0): event_type(event_type), comm_event(data.data(), EventDataType::COMM, 0){
    data[IDX_P] = 0; //program
    data[IDX_R] = 0; //rank
    data[IDX_T] = 0; //thread
    data[IDX_E] = event_id; //event
    data[COMM_IDX_TS]  = timestamp;
  }
  CommEvent(const CommEvent &r): event_type(r.event_type), data(r.data), comm_event(data.data(), EventDataType::COMM, 0){}
};

struct ADCommEventContainer{
  std::vector<CommEvent> events;
  std::unordered_map<int, std::string> event_types;
  ADEvent event_manager;

  int addEvent(const CommEvent &ev){ events.push_back(ev); return events.size() - 1; } //returns index of last added event

  void registerEvents(){
    for(int i=0;i<events.size();i++) event_types[ events[i].data[IDX_E] ] = events[i].event_type;
  }
  void linkEventType(){
    event_manager.linkEventType(&event_types);
  }
  void initializeAD(){
    registerEvents();
    linkEventType();
  }
  const Event_t & operator[](const int i) const{ return events[i].comm_event; };
};

TEST(ADEventTestaddComm, eventTypesNotAssigned){
  ADCommEventContainer ad;
  int idx = ad.addEvent(CommEvent("ACOMM"));
  ad.registerEvents();
  //ad.linkEventType();

  EXPECT_EQ( ad.event_manager.addComm(ad[idx]), EventError::UnknownEvent );
}
TEST(ADEventTestaddComm, eventNotInMap){
  ADCommEventContainer ad;
  int idx = ad.addEvent(CommEvent("ACOMM"));
  //ad.registerEvents();
  ad.linkEventType();

  EXPECT_EQ( ad.event_manager.addComm(ad[idx]), EventError::UnknownEvent );
}
TEST(ADEventTestaddComm, eventisnotSENDorRECV){
  ADCommEventContainer ad;
  int idx = ad.addEvent(CommEvent("INVALID_COMM"));
  ad.initializeAD();
  EXPECT_EQ( ad.event_manager.addComm(ad[idx]), EventError::UnknownEvent );
}

TEST(ADEventTestaddComm, eventisSEND){
  ADCommEventContainer ad;
  int idx = ad.addEvent(CommEvent("SEND"));
  ad.initializeAD();
  EXPECT_EQ( ad.event_manager.addComm(ad[idx]), EventError::OK );
}
TEST(ADEventTestaddComm, eventisRECV){
  ADCommEventContainer ad;
  int idx = ad.addEvent(CommEvent("RECV"));
  ad.initializeAD();
  EXPECT_EQ( ad.event_manager.addComm(ad[idx]), EventError::OK );
}





//Unit testing for ADEvent::trimCallList()
TEST(ADEventTesttrimCallList, trimsCorrectly){
  ADFuncEventContainer ad;
  long time = 100;

  int fid1=0;
  int fid2=1;

  int eid_entry = 0;
  int eid_exit = 1;

  int idx_entry = ad.addEvent(FuncEvent("ENTRY","MYFUNC" ,eid_entry,fid1,  0    ));
  int idx_exit = ad.addEvent(FuncEvent("EXIT","MYFUNC"   ,eid_exit,fid1,  time ));
  int idx_entry2 = ad.addEvent(FuncEvent("ENTRY","MYFUNC2",eid_entry,fid2,  time+50   )); //different function
  ad.initializeAD();
  EXPECT_EQ( ad.event_manager.addFunc(ad[idx_entry]), EventError::OK );
  EXPECT_EQ( ad.event_manager.addFunc(ad[idx_exit]), EventError::OK );
  EXPECT_EQ( ad.event_manager.addFunc(ad[idx_entry2]), EventError::OK );

  CallListMap_p_t* ret = ad.event_manager.trimCallList();

  int pid = 0;
  int rid = 0;
  int tid = 0;

  auto pit = ret->find(pid);
  EXPECT_NE(pit, ret->end() );

  auto rit = pit->second.find(rid);
  EXPECT_NE(rit, pit->second.end() );

  auto tit = rit->second.find(tid);
  EXPECT_NE(tit, rit->second.end() );

  const CallList_t &list = tit->second;

  EXPECT_EQ( list.size(), 1 );

  const ExecData_t &data = *list.begin();
  EXPECT_EQ( data.get_runtime(), time );
  EXPECT_EQ( data.get_fid(), fid1 );

}


TEST(ADEventTest, purgeCallList){
  ADFuncEventContainer ad;
  long time = 100;

  int fid1=0;
  int fid2=1;

  int eid_entry = 0;
  int eid_exit = 1;

  int idx_entry = ad.addEvent(FuncEvent("ENTRY","MYFUNC" ,eid_entry,fid1,  0    ));
  int idx_exit = ad.addEvent(FuncEvent("EXIT","MYFUNC"   ,eid_exit,fid1,  time ));
  int idx_entry2 = ad.addEvent(FuncEvent("ENTRY","MYFUNC2",eid_entry,fid2,  time+50   )); //different function
  ad.initializeAD();
  EXPECT_EQ( ad.event_manager.addFunc(ad[idx_entry]), EventError::OK );
  EXPECT_EQ( ad.event_manager.addFunc(ad[idx_exit]), EventError::OK );
  EXPECT_EQ( ad.event_manager.addFunc(ad[idx_entry2]), EventError::OK );

  ad.event_manager.purgeCallList();

  const CallList_t &call_list = ad.event_manager.getCallListMap()[0][0][0];
  EXPECT_EQ(call_list.size(), 1);
  EXPECT_EQ(call_list.begin()->get_fid(), fid2);
}



TEST(ADEvent, associatesCommsAndCountersWithFunc){
  Error().setStream(&std::cerr);

  ADEvent event_man;
  std::unordered_map<int, std::string> event_types = { {0,"ENTRY"}, {1,"EXIT"}, {2,"SEND"}, {3,"RECV"} };
  std::unordered_map<int, std::string> func_names = { {0,"MYFUNC"}, {1,"MYFUNCPARENT"} };
  std::unordered_map<int, std::string> counter_names = { {0,"MYCOUNTER"} };
  event_man.linkEventType(&event_types);
  event_man.linkFuncMap(&func_names);
  event_man.linkCounterMap(&counter_names);
  int ENTRY = 0;
  int EXIT = 1;
  int SEND = 2;
  int RECV = 3;
  int MYCOUNTER = 0;
  int MYFUNC = 0;
  int MYFUNCPARENT = 1;

  //Generate some ordered events. Put a counter and comm event before and after the function execution also to ensure that it doesn't pick those up
  int pid=0, tid=0, rid=0;

  std::vector<Event_t> events = {
    createFuncEvent_t(pid, rid, tid, ENTRY, MYFUNCPARENT, 99),

    createCounterEvent_t(pid, rid, tid, MYCOUNTER, 1234, 100),
    createCommEvent_t(pid, rid, tid, SEND, 0, 99, 1024, 110),
    createCounterEvent_t(pid, rid, tid, MYCOUNTER, 18783, 120), //same time as function entry, should get picked up by child

    createFuncEvent_t(pid, rid, tid, ENTRY, MYFUNC, 120),

    createCounterEvent_t(pid, rid, tid, MYCOUNTER, 1256, 130),
    createCounterEvent_t(pid, rid, tid, MYCOUNTER, 1267, 140),
    createCommEvent_t(pid, rid, tid, SEND, 1, 99, 1024, 150),
    createCommEvent_t(pid, rid, tid, RECV, 2, 99, 1024, 160),
    createCounterEvent_t(pid, rid, tid+1, MYCOUNTER, 1267, 170),  //not on same thread
    createCounterEvent_t(pid, rid, tid, MYCOUNTER, 1777, 180), //same time as function exit, should get picked up

    createFuncEvent_t(pid, rid, tid, EXIT, MYFUNC, 180),

    createCommEvent_t(pid, rid, tid, SEND, 3, 99, 1024, 190),

    createFuncEvent_t(pid, rid, tid, EXIT, MYFUNCPARENT, 191)    
  };
  for(int i=0;i<events.size();i++)
    event_man.addEvent(events[i]);

  const ExecDataMap_t &func_calls_by_idx = *event_man.getExecDataMap();
    
  //Check for child function
  EXPECT_NE(func_calls_by_idx.find(MYFUNC), func_calls_by_idx.end());

  const std::vector<CallListIterator_t> &myfunc_calls = func_calls_by_idx.find(MYFUNC)->second;
  EXPECT_EQ(myfunc_calls.size(), 1);

  const ExecData_t &myfunc_exec = *myfunc_calls[0];
  eventID exec_id = myfunc_exec.get_id();

  EXPECT_EQ(myfunc_exec.get_funcname(), "MYFUNC");
  EXPECT_EQ(myfunc_exec.get_n_message(), 2);
  EXPECT_EQ(myfunc_exec.get_messages()[0].ts(), 150);
  EXPECT_EQ(myfunc_exec.get_messages()[0].get_exec_key(), exec_id);


  std::cout << "Found " << myfunc_exec.get_n_counter() << " counters:" << std::endl;
  for(int i=0;i<myfunc_exec.get_n_counter();i++)
    std::cout << i << ": " << myfunc_exec.get_counters()[i].get_json().dump() << std::endl;


  EXPECT_EQ(myfunc_exec.get_n_counter(), 4);
  EXPECT_EQ(myfunc_exec.get_counters()[0].get_ts(), 120);
  EXPECT_EQ(myfunc_exec.get_counters()[0].get_exec_key(), exec_id);

  //Check for parent function
  EXPECT_NE(func_calls_by_idx.find(MYFUNCPARENT), func_calls_by_idx.end());

  const std::vector<CallListIterator_t> &myfuncparent_calls = func_calls_by_idx.find(MYFUNCPARENT)->second;
  EXPECT_EQ(myfuncparent_calls.size(), 1);

  const ExecData_t &myfuncparent_exec = *myfuncparent_calls[0];
  eventID exec_id2 = myfuncparent_exec.get_id();

  EXPECT_EQ(myfuncparent_exec.get_funcname(), "MYFUNCPARENT");
  EXPECT_EQ(myfuncparent_exec.get_n_message(), 2);
  EXPECT_EQ(myfuncparent_exec.get_messages()[0].ts(), 110);
  EXPECT_EQ(myfuncparent_exec.get_messages()[0].get_exec_key(), exec_id2);


  std::cout << "Found " << myfuncparent_exec.get_n_counter() << " counters:" << std::endl;
  for(int i=0;i<myfuncparent_exec.get_n_counter();i++)
    std::cout << i << ": " << myfuncparent_exec.get_counters()[i].get_json().dump() << std::endl;


  EXPECT_EQ(myfuncparent_exec.get_n_counter(), 1);
  EXPECT_EQ(myfuncparent_exec.get_counters()[0].get_ts(), 100);
  EXPECT_EQ(myfuncparent_exec.get_counters()[0].get_exec_key(), exec_id2);
}


TEST(ADEvent, trimsCallListCorrectly){
  ADEvent event_man;

  int pid = 1;
  int tid = 2;
  int rid = 3;
  int func_id[] = {4,5};
  std::string func_name[] = {"hello_world", "goodbye_world"};

  //Add 2 events and check added properly
  ExecData_t c1 = createFuncExecData_t(pid, rid, tid, func_id[0], func_name[0], 100, 200);
  ExecData_t c2 = createFuncExecData_t(pid, rid, tid, func_id[1], func_name[1], 300, 400);
  event_man.addCall(c1);
  event_man.addCall(c2);

  auto const* calls_p_r_t_ptr = getElemPRT(pid,rid,tid,event_man.getCallListMap());
  EXPECT_NE(calls_p_r_t_ptr, nullptr);
  const CallList_t &calls_p_r_t =  *calls_p_r_t_ptr;
  EXPECT_EQ(calls_p_r_t.size(), 2);

  //Check trim with n_keep >= #elems does nothing
  event_man.trimCallList(2);
  EXPECT_EQ(calls_p_r_t.size(), 2);
  event_man.trimCallList(3);
  EXPECT_EQ(calls_p_r_t.size(), 2);

  //Check purged events are correct
  CallListMap_p_t* purged = event_man.trimCallList();
  EXPECT_EQ(calls_p_r_t.size(), 0);

  auto const* purged_calls_p_r_t_ptr = getElemPRT(pid,rid,tid,*purged);
  EXPECT_NE(purged_calls_p_r_t_ptr, nullptr);

  const CallList_t &purged_calls_p_r_t = *purged_calls_p_r_t_ptr;
  EXPECT_EQ(purged_calls_p_r_t.size(), 2);

  EXPECT_EQ(purged_calls_p_r_t.begin()->get_funcname(), func_name[0]);
  EXPECT_EQ(std::next(purged_calls_p_r_t.begin(),1)->get_funcname(), func_name[1]);
  delete purged;
}


TEST(ADEvent, trimsCallListCorrectlyWithGCFlag){
  ADEvent event_man;

  int pid = 1;
  int tid = 2;
  int rid = 3;
  int func_id[] = {4,5};
  std::string func_name[] = {"hello_world", "goodbye_world"};

  //Add 2 events and check added properly
  ExecData_t c1 = createFuncExecData_t(pid, rid, tid, func_id[0], func_name[0], 100, 200);
  ExecData_t c2 = createFuncExecData_t(pid, rid, tid, func_id[1], func_name[1], 300, 400);

  //Add 2 events but mark 1 to be undeletable
  event_man.addCall(c1);
  c2.register_reference();
  EXPECT_EQ(c2.can_delete(), false);
  EXPECT_EQ(c2.reference_count(), 1);

  event_man.addCall(c2);

  auto const* calls_p_r_t_ptr = getElemPRT(pid,rid,tid,event_man.getCallListMap());
  EXPECT_NE(calls_p_r_t_ptr, nullptr);
  const CallList_t &calls_p_r_t =  *calls_p_r_t_ptr;
  EXPECT_EQ(calls_p_r_t.size(), 2);


  //Check purged events are correct
  CallListMap_p_t* purged = event_man.trimCallList();
  EXPECT_EQ(calls_p_r_t.size(), 1);

  auto const* purged_calls_p_r_t_ptr = getElemPRT(pid,rid,tid,*purged);
  EXPECT_NE(purged_calls_p_r_t_ptr, nullptr);

  const CallList_t &purged_calls_p_r_t = *purged_calls_p_r_t_ptr;
  EXPECT_EQ(purged_calls_p_r_t.size(), 1);

  EXPECT_EQ(purged_calls_p_r_t.begin()->get_funcname(), func_name[0]);
  delete purged;
}

TEST(ADEvent, matchesEventsByCorrelationID){
  ADEvent event_man;
  int pid = 1;
  int rid = 2;
  int tid_cpu = 3;
  int tid_gpu = 4;
  int counter_id = 13; //index of "Correlation ID" counter

  //Have the CPU function execute 2 GPU kernels
  ExecData_t c_cpu = createFuncExecData_t(pid, rid, tid_cpu, 4, "cpu_launch_kernel", 100, 200); //100-200
  c_cpu.add_counter(createCounterData_t(pid,rid,tid_cpu,counter_id, 1995, 100, "Correlation ID"));
  c_cpu.add_counter(createCounterData_t(pid,rid,tid_cpu,counter_id, 2020, 150, "Correlation ID"));

  ExecData_t c_cpu_gpu1 = createFuncExecData_t(pid, rid, tid_gpu, 5, "gpu_kernel", 400, 100); //400-500
  c_cpu_gpu1.add_counter(createCounterData_t(pid,rid,tid_gpu,counter_id, 1995, 500, "Correlation ID"));

  ExecData_t c_cpu_gpu2 = createFuncExecData_t(pid, rid, tid_gpu, 6, "gpu_kernel2", 500, 100); //500-600
  c_cpu_gpu2.add_counter(createCounterData_t(pid,rid,tid_gpu,counter_id, 2020, 600, "Correlation ID"));

  //Have a CPU parent call that also executes a GPU kernel, but the matching occurs after the child function has been matched
  //Need to test that the GC doesn't erase the parent event when the child event is matched but the parent has not yet been matched
  ExecData_t c_cpu_p1 = createFuncExecData_t(pid, rid, tid_cpu, 7, "cpu_launch_kernel_parent", 50, 250); //50-250
  bindParentChild(c_cpu_p1, c_cpu);
  c_cpu_p1.add_counter(createCounterData_t(pid,rid,tid_cpu,counter_id, 1885, 150, "Correlation ID"));

  ExecData_t c_cpu_p1_gpu = createFuncExecData_t(pid, rid, tid_gpu, 8, "gpu_kernel_of_parent", 400, 100); //400-500
  c_cpu_p1_gpu.add_counter(createCounterData_t(pid,rid,tid_gpu,counter_id, 1885, 500, "Correlation ID"));

  //A grandparent with no gpu kernels
  ExecData_t c_cpu_p2 = createFuncExecData_t(pid, rid, tid_cpu, 7, "cpu_launch_kernel_grandparent", 0, 300); //0-300
  bindParentChild(c_cpu_p2, c_cpu_p1);

  std::cout << "c_cpu_p2 : " << c_cpu_p2.get_id().toString() << std::endl;
  std::cout << "c_cpu_p1 : " << c_cpu_p1.get_id().toString() << std::endl;
  std::cout << "c_cpu : " << c_cpu.get_id().toString() << std::endl;


  auto c_cpu_p2_it = event_man.addCall(c_cpu_p2);

  //c_cpu should not be locked yet
  ASSERT_EQ(c_cpu_p2_it->reference_count(), 0);

  auto c_cpu_p1_it = event_man.addCall(c_cpu_p1);
  
  //This should lock both c_cpu_p2 and c_cpu_p1
  ASSERT_EQ(c_cpu_p2_it->reference_count(), 1);
  ASSERT_EQ(c_cpu_p1_it->reference_count(), 1);

  auto c_cpu_it = event_man.addCall(c_cpu);

  //c_cpu should be doubly locked
  ASSERT_EQ(c_cpu_it->reference_count(), 2);
  //c_cpu_p1 should be triply locked
  ASSERT_EQ(c_cpu_p1_it->reference_count(), 3);
  //c_cpu_p2 should be triply locked
  ASSERT_EQ(c_cpu_p2_it->reference_count(), 3);

  //Ensure the correlation IDs got picked up
  EXPECT_EQ( event_man.getUnmatchCorrelationIDevents().size(), 3 );

  //Ensure the unmatched event doesn't get deleted by trimming
  delete event_man.trimCallList();

  const CallListMap_p_t &calls = event_man.getCallListMap();
  CallList_t const* calls_p_r_t_cpu_ptr = getElemPRT(pid,rid,tid_cpu, calls);
  EXPECT_NE(calls_p_r_t_cpu_ptr, nullptr);
  const CallList_t &calls_p_r_t_cpu = *calls_p_r_t_cpu_ptr;

  EXPECT_EQ( calls_p_r_t_cpu.size(), 3);

  event_man.addCall(c_cpu_gpu1);

  CallList_t const* calls_p_r_t_gpu_ptr = getElemPRT(pid,rid,tid_gpu,calls);
  EXPECT_NE(calls_p_r_t_gpu_ptr, nullptr);
  const CallList_t &calls_p_r_t_gpu = *calls_p_r_t_gpu_ptr;

  EXPECT_EQ( calls_p_r_t_gpu.size(), 1); //only first event present now

  auto cpu_it = std::next(calls_p_r_t_cpu.begin(),2);
  auto gpu_it = calls_p_r_t_gpu.begin();

  EXPECT_EQ( cpu_it->n_GPU_correlationID_partner(), 1); //only 1 matched at this point
  EXPECT_EQ( cpu_it->get_GPU_correlationID_partner(0), c_cpu_gpu1.get_id() );
  EXPECT_EQ( gpu_it->n_GPU_correlationID_partner(), 1);
  EXPECT_EQ( gpu_it->get_GPU_correlationID_partner(0), c_cpu.get_id() );

  EXPECT_EQ( event_man.getUnmatchCorrelationIDevents().size(), 2); //1 gpu event of c_cpu and 1 of c_cpu_p1 remain

  //Make sure gpu event trimmed but not cpu event
  delete event_man.trimCallList();
  EXPECT_EQ( calls_p_r_t_cpu.size(), 3);
  EXPECT_EQ( calls_p_r_t_gpu.size(), 0);

  event_man.addCall(c_cpu_gpu2);
  EXPECT_EQ( calls_p_r_t_gpu.size(), 1);

  gpu_it = calls_p_r_t_gpu.begin();
  EXPECT_EQ( cpu_it->n_GPU_correlationID_partner(), 2); //both gpu kernels of c_cpu now matched
  EXPECT_EQ( cpu_it->get_GPU_correlationID_partner(1), c_cpu_gpu2.get_id() );
  EXPECT_EQ( gpu_it->n_GPU_correlationID_partner(), 1);
  EXPECT_EQ( gpu_it->get_GPU_correlationID_partner(0), c_cpu.get_id() );

  EXPECT_EQ( event_man.getUnmatchCorrelationIDevents().size(), 1 ); //parent still hasn't been matched

  //Ensure c_cpu and both of it's gpu events are now trimmed out
  delete event_man.trimCallList();
  EXPECT_EQ( calls_p_r_t_cpu.size(), 2); //parent and grandparent remain
  EXPECT_EQ( calls_p_r_t_gpu.size(), 0);


  //Now add the parent's gpu kernel
  event_man.addCall(c_cpu_p1_gpu);
  EXPECT_EQ( calls_p_r_t_gpu.size(), 1);

  cpu_it = std::next(calls_p_r_t_cpu.begin(),1);
  gpu_it = calls_p_r_t_gpu.begin();
  EXPECT_EQ( cpu_it->n_GPU_correlationID_partner(), 1);
  EXPECT_EQ( cpu_it->get_GPU_correlationID_partner(0), c_cpu_p1_gpu.get_id() );
  EXPECT_EQ( gpu_it->n_GPU_correlationID_partner(), 1);
  EXPECT_EQ( gpu_it->get_GPU_correlationID_partner(0), c_cpu_p1.get_id() );

  EXPECT_EQ( event_man.getUnmatchCorrelationIDevents().size(), 0 );

  //All events are now trimmed out
  delete event_man.trimCallList();
  EXPECT_EQ( calls_p_r_t_cpu.size(), 0);
  EXPECT_EQ( calls_p_r_t_gpu.size(), 0);
}




TEST(ADEvent, testCoridMatchChainUnlock){
  //Ensure that for a tree with multiple GPU events and common parents, when unlocking the tree for the first event
  //that we don't allow the common parents to get unlocked before the second event is matched

  ADEvent event_man;
  int pid = 1;
  int rid = 2;
  int tid_cpu = 3;
  int tid_gpu = 4;
  int counter_id = 13; //index of "Correlation ID" counter

  //First CPU event
  ExecData_t c_cpu_1 = createFuncExecData_t(pid, rid, tid_cpu, 4, "cpu_launch_kernel_1", 100, 200); //100-300
  c_cpu_1.add_counter(createCounterData_t(pid,rid,tid_cpu,counter_id, 1995, 100, "Correlation ID"));

  ExecData_t c_gpu1 = createFuncExecData_t(pid, rid, tid_gpu, 6, "gpu_kernel1", 400, 100); //400-500
  c_gpu1.add_counter(createCounterData_t(pid,rid,tid_gpu,counter_id, 1995, 500, "Correlation ID"));

  //Second CPU event
  ExecData_t c_cpu_2 = createFuncExecData_t(pid, rid, tid_cpu, 5, "cpu_launch_kernel_2", 150, 250); //150-250
  c_cpu_2.add_counter(createCounterData_t(pid,rid,tid_cpu,counter_id, 2020, 150, "Correlation ID"));

  ExecData_t c_gpu2 = createFuncExecData_t(pid, rid, tid_gpu, 7, "gpu_kernel2", 500, 100); //500-600
  c_gpu2.add_counter(createCounterData_t(pid,rid,tid_gpu,counter_id, 2020, 600, "Correlation ID"));

  //Common parent event
  ExecData_t c_cpu_p1 = createFuncExecData_t(pid, rid, tid_cpu, 8, "cpu_launch_kernel_parent", 50, 500); //50-550
  bindParentChild(c_cpu_p1, c_cpu_1);
  bindParentChild(c_cpu_p1, c_cpu_2);

  auto c_cpu_p1_it =  event_man.addCall(c_cpu_p1);
  auto c_cpu_1_it = event_man.addCall(c_cpu_1);
  auto c_cpu_2_it = event_man.addCall(c_cpu_2);

  //Ensure the correlation IDs got picked up
  EXPECT_EQ( event_man.getUnmatchCorrelationIDevents().size(), 2 );

  //Both the CPU and parent event should be locked
  EXPECT_EQ( c_cpu_p1_it->can_delete(), false );
  EXPECT_EQ( c_cpu_1_it->can_delete(), false );
  EXPECT_EQ( c_cpu_2_it->can_delete(), false );


  //Add the first GPU event
  auto c_gpu1_it = event_man.addCall(c_gpu1);

  //Check correlation ID was matched
  EXPECT_EQ( event_man.getUnmatchCorrelationIDevents().size(), 1 );

  //Check both gpu1 and cpu1 are unlocked
  EXPECT_EQ( c_cpu_1_it->can_delete(), true );
  EXPECT_EQ( c_gpu1_it->can_delete(), true );

  //Check parent is *not* unlocked
  EXPECT_EQ( c_cpu_p1_it->can_delete(), false ); 
}



TEST(ADEvent, testIgnoreFunctionCorrelationID){
  int pid = 1;
  int rid = 2;
  int tid_cpu = 3;
  int tid_gpu = 4;
  int counter_id = 13; //index of "Correlation ID" counter

  //Have the CPU function execute 2 GPU kernels
  ExecData_t c_cpu = createFuncExecData_t(pid, rid, tid_cpu, 4, "cpu_launch_kernel", 100, 200); //100-200
  c_cpu.add_counter(createCounterData_t(pid,rid,tid_cpu,counter_id, 1995, 100, "Correlation ID"));

  ExecData_t c_cpu_gpu1 = createFuncExecData_t(pid, rid, tid_gpu, 5, "gpu_kernel", 400, 100); //400-500
  c_cpu_gpu1.add_counter(createCounterData_t(pid,rid,tid_gpu,counter_id, 1995, 500, "Correlation ID"));

  //Check without ignoring first
  {
    ADEvent event_man;
    auto c_cpu_it = event_man.addCall(c_cpu);
    EXPECT_EQ( event_man.getUnmatchCorrelationIDevents().size(), 1 );
    
    auto c_cpu_gpu1_it = event_man.addCall(c_cpu_gpu1);
    EXPECT_EQ( event_man.getUnmatchCorrelationIDevents().size(), 0 );
  }
  //Check with ignoring cpu func
  {
    ADEvent event_man;
    event_man.ignoreCorrelationIDsForFunction("cpu_launch_kernel");
    auto c_cpu_it = event_man.addCall(c_cpu);
    EXPECT_EQ( event_man.getUnmatchCorrelationIDevents().size(), 0 );
    
    auto c_cpu_gpu1_it = event_man.addCall(c_cpu_gpu1);
    EXPECT_EQ( event_man.getUnmatchCorrelationIDevents().size(), 1 );
  }

  //Check with ignoring gpu func
  {
    ADEvent event_man;
    event_man.ignoreCorrelationIDsForFunction("gpu_kernel");
    auto c_cpu_it = event_man.addCall(c_cpu);
    EXPECT_EQ( event_man.getUnmatchCorrelationIDevents().size(), 1 );
    
    auto c_cpu_gpu1_it = event_man.addCall(c_cpu_gpu1);
    EXPECT_EQ( event_man.getUnmatchCorrelationIDevents().size(), 1 );
  }

  //Check with ignoring both
  {
    ADEvent event_man;
    event_man.ignoreCorrelationIDsForFunction("gpu_kernel");
    event_man.ignoreCorrelationIDsForFunction("cpu_launch_kernel");

    auto c_cpu_it = event_man.addCall(c_cpu);
    EXPECT_EQ( event_man.getUnmatchCorrelationIDevents().size(), 0 );
    
    auto c_cpu_gpu1_it = event_man.addCall(c_cpu_gpu1);
    EXPECT_EQ( event_man.getUnmatchCorrelationIDevents().size(), 0 );
  }
}


TEST(ADEvent, testIteratorWindowDetermination){
  ADEvent event_man;
  int pid=0, rid=0, tid_func=0, tid_other=1;

  std::vector<ExecData_t> execs;
  execs.push_back(createFuncExecData_t(pid, rid, tid_func, 1, "func1", 100, 100)); //0
  execs.push_back(createFuncExecData_t(pid, rid, tid_func, 2, "func2", 200, 100)); //1
  execs.push_back(createFuncExecData_t(pid, rid, tid_func, 3, "func3", 400, 100)); //2
  execs.push_back(createFuncExecData_t(pid, rid, tid_func, 4, "func4", 500, 100)); //3  this will be the window center
  execs.push_back(createFuncExecData_t(pid, rid, tid_other, 5, "func5", 600, 100)); //different thread, shouldn't be in window
  execs.push_back(createFuncExecData_t(pid, rid, tid_func, 6, "func6", 700, 100)); //4
  execs.push_back(createFuncExecData_t(pid, rid, tid_func, 7, "func7", 800, 100)); //5
  execs.push_back(createFuncExecData_t(pid, rid, tid_func, 8, "func8", 900, 100)); //6

  for(int i=0;i<execs.size();i++)
    event_man.addCall(execs[i]);

  CallList_t* call_list = getElemPRT(pid,rid,tid_func, event_man.getCallListMap());
  EXPECT_NE(call_list, nullptr);
  EXPECT_EQ(call_list->size(), execs.size()-1); //one event on different thread

  CallListIterator_t begin = call_list->begin();
  CallListIterator_t end = call_list->end();

  std::pair<CallListIterator_t, CallListIterator_t> it_p = event_man.getCallWindowStartEnd(execs[3].get_id(), 3);
  EXPECT_EQ(it_p.first, begin);
  EXPECT_EQ(it_p.second, end); //second iterator is *one past the end* of the window

  it_p = event_man.getCallWindowStartEnd(execs[3].get_id(), 4); //should stop at edge of map
  EXPECT_EQ(it_p.first, begin);
  EXPECT_EQ(it_p.second, end);

  //Check that trimming but keeping 4 events per thread means we get the full upper half of the window view
  event_man.trimCallList(4);
  EXPECT_EQ(call_list->size(), 4);

  it_p = event_man.getCallWindowStartEnd(execs[3].get_id(), 3);
  EXPECT_EQ(it_p.first->get_id(), execs[3].get_id());
  EXPECT_EQ(std::prev(it_p.second,1)->get_id(), execs[7].get_id());

  //Add some more data simulating the next io step
  execs.push_back(createFuncExecData_t(pid, rid, tid_func, 9, "func9", 1000, 100)); //execs[8]
  execs.push_back(createFuncExecData_t(pid, rid, tid_func, 10, "func10", 1100, 100)); //execs[9]

  for(int i=8;i<execs.size();i++)
    event_man.addCall(execs[i]);


  it_p = event_man.getCallWindowStartEnd(execs[8].get_id(), 1); //window of 1 around execs[8]
  EXPECT_EQ(it_p.first->get_id(), execs[7].get_id());
  EXPECT_EQ(std::prev(it_p.second,1)->get_id(), execs[9].get_id());

}
  
TEST(ADEventTest, DetectsCorrelationIDerrors){
  ADEvent event_man;
    
  std::unordered_map<int, std::string> event_types = { {0, "ENTRY"}, {1, "EXIT" } };
  std::unordered_map<int, std::string> func_names = { {111, "cpu_func"}, {222, "gpu_func" } };
  std::unordered_map<int, std::string> counter_names = { {0, "Correlation ID"} };
  std::unordered_map<unsigned long, GPUvirtualThreadInfo> gpu_threads = { {6, GPUvirtualThreadInfo(1,2,3,4) } };

  event_man.linkEventType(&event_types);
  event_man.linkFuncMap(&func_names);
  event_man.linkCounterMap(&counter_names);
  event_man.linkGPUthreadMap(&gpu_threads);
  
  Event_t cpu_entry = createFuncEvent_t(0,0,0, 0, 111, 1000);
  Event_t cpu_exit = createFuncEvent_t(0,0,0, 1, 111, 2000);

  Event_t cpu_corrid1 = createCounterEvent_t(0,0,0,  0,  99,  1200);
  Event_t cpu_corrid2 = createCounterEvent_t(0,0,0,  0,  500,  1999);  

  Event_t gpu_entry = createFuncEvent_t(0,0,6, 0, 222, 1200);
  Event_t gpu_exit = createFuncEvent_t(0,0,6, 1, 222, 1800);

  Event_t gpu_corrid1 = createCounterEvent_t(0,0,6,  0,  99,  1799);
  Event_t gpu_corrid2 = createCounterEvent_t(0,0,6,  0,  100,  1800);

  event_man.addFunc(cpu_entry);

  event_man.addFunc(gpu_entry);
  event_man.addCounter(cpu_corrid1);

  event_man.addCounter(gpu_corrid1);
  event_man.addCounter(gpu_corrid2);

  //GPU events not allowed to have multiple correlation IDs (non-fatal)
  {
    std::stringstream err_str;
    Error().setStream(&err_str);

    event_man.addFunc(gpu_exit);

    std::string got = err_str.str();
    size_t loc = got.find("Encountered a GPU kernel execution with multiple correlation IDs!");
    std::cout << "Got intentional error: " << got << std::endl;
    EXPECT_NE(loc, std::string::npos);
  }

  //CPU events are allowed to have multiple correlation IDs
  {
    event_man.addCounter(cpu_corrid2);
    
    std::stringstream err_str;
    Error().setStream(&err_str);

    event_man.addFunc(cpu_exit);

    std::string got = err_str.str();
    size_t loc = got.find("Encountered a GPU kernel execution with multiple correlation IDs!");
    std::cout << "This should be empty: " << got << std::endl;
    EXPECT_EQ(loc, std::string::npos);
  }
}


TEST(ADEventTest, TestCallStackExtraction){
  ExecData_t exec0 = createFuncExecData_t(1,2,3, 55, "theroot", 100, 0);  //0 runtime indicates it has yet to complete
  ExecData_t exec1 = createFuncExecData_t(1,2,3, 55, "theparent", 1000, 100);
  ExecData_t exec2 = createFuncExecData_t(1,2,3, 77, "thechild", 1050, 50); //going to be the anomalous one
  exec2.set_parent(exec1.get_id());
  exec1.set_parent(exec0.get_id());

  ADEvent event_man;
  event_man.addCall(exec0);
  event_man.addCall(exec1);
  event_man.addCall(exec2);
  
  std::vector<CallListIterator_t> cs = event_man.getCallStack(exec2.get_id());
  
  EXPECT_EQ(cs.size(),3);
  EXPECT_EQ(cs[0]->get_id(), exec2.get_id());
  EXPECT_EQ(cs[1]->get_id(), exec1.get_id());
  EXPECT_EQ(cs[2]->get_id(), exec0.get_id());
}
