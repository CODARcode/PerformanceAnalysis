#include<chimbuko/ad/ADEvent.hpp>
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
    for(int i=0;i<events.size();i++) event_types[ events[i].data[IDX_E] ] = events[i].event_type;
  }
  void registerFuncs(){
    for(int i=0;i<events.size();i++) func_map[ events[i].data[FUNC_IDX_F] ] = events[i].func_name;
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
  
  int idx_entry = ad.addEvent(FuncEvent("ENTRY","MYFUNC",1,1,0));
  int idx_exit = ad.addEvent(FuncEvent("EXIT","MYFUNC",2,1,time));
  int idx_entry2 = ad.addEvent(FuncEvent("ENTRY","MYFUNC",3,2)); //different function
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
  
}

TEST(ADEvent, associatesCommsAndCountersWithFunc){
  ADEvent event_man;
  std::unordered_map<int, std::string> event_types = { {0,"ENTRY"}, {1,"EXIT"}, {2,"SEND"}, {3,"RECV"} };
  std::unordered_map<int, std::string> func_names = { {0,"MYFUNC"} };
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

  //Generate some ordered events. Put a counter and comm event before and after the function execution also to ensure that it doesn't pick those up

  // Event_t createCounterEvent_t(unsigned long pid,
  // 					 unsigned long rid,
  // 					 unsigned long tid,
  // 					 unsigned long counter_id,
  // 					 unsigned long value,
  // 					 unsigned long ts){

  // Event_t createFuncEvent_t(unsigned long pid,
  // 				      unsigned long rid,
  // 				      unsigned long tid,
  // 				      unsigned long eid,
  // 				      unsigned long func_id,
  // 				      unsigned long ts){

  // Event_t createCommEvent_t(unsigned long pid,
  // 			    unsigned long rid,
  // 			    unsigned long tid,
  // 			    unsigned long eid,
  // 			    unsigned long comm_tag,
  // 			    unsigned long comm_partner,
  // 			    unsigned long comm_bytes,
  // 			    unsigned long ts){
  int pid=0, tid=0, rid=0;

  std::vector<Event_t> events = {
    createCounterEvent_t(pid, rid, tid, MYCOUNTER, 1234, 100),
    createCommEvent_t(pid, rid, tid, SEND, 0, 99, 1024, 110),
    createFuncEvent_t(pid, rid, tid, ENTRY, MYFUNC, 120),
    createCounterEvent_t(pid, rid, tid, MYCOUNTER, 1256, 130),
    createCounterEvent_t(pid, rid, tid, MYCOUNTER, 1267, 140),
    createCommEvent_t(pid, rid, tid, SEND, 1, 99, 1024, 150),
    createCommEvent_t(pid, rid, tid, RECV, 2, 99, 1024, 160),
    createCounterEvent_t(pid, rid, tid+1, MYCOUNTER, 1267, 170),  //not on same thread
    createFuncEvent_t(pid, rid, tid, EXIT, MYFUNC, 180),
    createCommEvent_t(pid, rid, tid, SEND, 3, 99, 1024, 190)
  };
  for(int i=0;i<events.size();i++)
    event_man.addEvent(events[i]);

  const ExecDataMap_t &func_calls_by_idx = *event_man.getExecDataMap();
  EXPECT_NE(func_calls_by_idx.find(MYFUNC), func_calls_by_idx.end());
  
  const std::vector<CallListIterator_t> &myfunc_calls = func_calls_by_idx.find(MYFUNC)->second;
  EXPECT_EQ(myfunc_calls.size(), 1);

  const ExecData_t &myfunc_exec = *myfunc_calls[0];
  std::string exec_id = myfunc_exec.get_id();

  EXPECT_EQ(myfunc_exec.get_funcname(), "MYFUNC");
  EXPECT_EQ(myfunc_exec.get_n_message(), 2);
  EXPECT_EQ(myfunc_exec.get_messages()[0].ts(), 150);
  EXPECT_EQ(myfunc_exec.get_messages()[0].get_exec_key(), exec_id);
  EXPECT_EQ(myfunc_exec.get_n_counter(), 2);
  EXPECT_EQ(myfunc_exec.get_counters()[0].get_ts(), 130);
  EXPECT_EQ(myfunc_exec.get_counters()[0].get_exec_key(), exec_id);
}


TEST(ADEvent, trimsCallListCorrectly){
  ADEvent event_man;
  //std::unordered_map<int, std::string> func_names = { {0,"MYFUNC"} };
  //event_man.linkFuncMap(&func_names);
  
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
  
  const CallListMap_p_t &calls = event_man.getCallListMap();
  auto p = calls.find(pid);
  EXPECT_EQ(p != calls.end(), true);
  auto r = p->second.find(rid);
  EXPECT_EQ(r != p->second.end(), true);
  auto t = r->second.find(tid);
  EXPECT_EQ(t != r->second.end(), true);
  
  const CallList_t &calls_p_r_t = t->second;
  EXPECT_EQ(calls_p_r_t.size(), 2);

  //Check purged events are correct
  {
    CallListMap_p_t* purged = event_man.trimCallList();
    EXPECT_EQ(calls_p_r_t.size(), 0);
    
    auto zp = purged->find(pid);
    EXPECT_EQ(zp != purged->end(), true);
    auto zr = zp->second.find(rid);
    EXPECT_EQ(zr != zp->second.end(), true);
    auto zt = zr->second.find(tid);
    EXPECT_EQ(zt != zr->second.end(), true);
    
    const CallList_t &purged_calls_p_r_t = zt->second;
    EXPECT_EQ(purged_calls_p_r_t.size(), 2);
    
    EXPECT_EQ(purged_calls_p_r_t.begin()->get_funcname(), func_name[0]);
    EXPECT_EQ(std::next(purged_calls_p_r_t.begin(),1)->get_funcname(), func_name[1]);
    delete purged;
  }
  
  //Add 2 events but mark 1 to be undeletable
  event_man.addCall(c1);
  c2.can_delete(false);
  event_man.addCall(c2);
  EXPECT_EQ(calls_p_r_t.size(), 2);
  
  //Check purged events are correct
  {
    CallListMap_p_t* purged = event_man.trimCallList();
    EXPECT_EQ(calls_p_r_t.size(), 1);
    
    auto zp = purged->find(pid);
    EXPECT_EQ(zp != purged->end(), true);
    auto zr = zp->second.find(rid);
    EXPECT_EQ(zr != zp->second.end(), true);
    auto zt = zr->second.find(tid);
    EXPECT_EQ(zt != zr->second.end(), true);
    
    const CallList_t &purged_calls_p_r_t = zt->second;
    EXPECT_EQ(purged_calls_p_r_t.size(), 1);
    
    EXPECT_EQ(purged_calls_p_r_t.begin()->get_funcname(), func_name[0]);
    delete purged;
  }


}
  
