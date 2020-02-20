#include<chimbuko/ad/ADEvent.hpp>
#include "gtest/gtest.h"

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
