#pragma once
#include "ddb_wrapper.h"

struct provDBtables{
  Table anomalies;
  Table call_stacks;
  Table call_stack_entry_events;
  Table counter_values;
  Table io_steps;
  
  provDBtables(duckdb_connection con): anomalies("anomalies"), call_stacks("call_stacks"), call_stack_entry_events("call_stack_entry_events"), counter_values("counter_values"), io_steps("io_steps"){
    //Define the schemas
#define DEF(T,NM) tab.addColumn<T>(#NM)
    
    {
      auto &tab = anomalies;
      DEF(std::string, event_id); //primary
      DEF(uint64_t,entry);
      DEF(uint64_t,exit);
      DEF(int,fid);
      DEF(int,pid);
      DEF(int,rid);
      DEF(int,tid);
      DEF(int,io_step_index);
      DEF(uint64_t,runtime_exclusive);
      DEF(double,outlier_score);
      DEF(double,outlier_severity);    
      tab.define(con);
    }
    {
      auto &tab = call_stacks;
      DEF(std::string,event_key); //foreign, references "event_id" in table:"anomalies"
      DEF(std::string,call_stack_entry_event_id); //references an event in table:"call_stack_entry_events"
      tab.define(con);
    }
    {
      auto &tab = call_stack_entry_events;
      DEF(std::string,call_stack_entry_event_id); //primary
      DEF(uint64_t,entry);
      DEF(uint64_t,exit);
      DEF(int,fid);
      tab.define(con);
    }
    {
      auto &tab = counter_values;
      DEF(std::string,event_id);
      DEF(int,counter_idx);
      DEF(uint64_t,counter_value);
      DEF(uint64_t,timestamp);
      DEF(int,pid);
      DEF(int,rid);  
      DEF(int,tid);
      tab.define(con);
    }
    {
      auto &tab = io_steps;
      DEF(int, io_step_index);
      DEF(int, io_step);
      DEF(int, pid);
      DEF(int, rid);
      DEF(uint64_t, io_step_tstart);
      DEF(uint64_t, io_step_tend);
      tab.define(con);
    }
    
  }
  void write(duckdb_connection con){
    anomalies.write(con);
    call_stacks.write(con);
    call_stack_entry_events.write(con);
    counter_values.write(con);
    io_steps.write(con);
  }
  void clear(){
    anomalies.clear();
    call_stacks.clear();
    call_stack_entry_events.clear();
    counter_values.clear();
    io_steps.clear();
  }
  
};

std::string uniqueEventID(){
  static int i=0;
  return std::to_string(i++);
}
  
#define INS(A) tab(r,#A) = A;

//The events appearing in the call_stack
//In reality, call stack entries can be shared between anomalies, but for convenience here we just assume they are unique and attach them to the entries in the call stack lists
struct CallStackEntryEventsEntry{
  uint64_t entry;
  uint64_t exit;
  int fid;

  void insert(provDBtables &tables, const std::string &call_stack_entry_event_id) const{
    auto &tab = tables.call_stack_entry_events;
    int r = tab.addRow();
    INS(call_stack_entry_event_id);
    INS(entry);
    INS(exit);
    INS(fid);
  }
  void fakeEntry(){
    entry = 787;
    exit = 899;
    fid = 19;
  }
  
};

struct CallStacksEntry{
  std::string call_stack_entry_event_id; //an entry that appears in the table of call stack events
  CallStackEntryEventsEntry call_stack_entry; //cf note above
  
  void insert(provDBtables &tables, const std::string &event_key) const{
    auto &tab = tables.call_stacks;
    int r = tab.addRow();
    INS(event_key);
    INS(call_stack_entry_event_id);
    call_stack_entry.insert(tables, call_stack_entry_event_id);
  }
  void fakeEntry(){
    call_stack_entry_event_id = uniqueEventID();
    call_stack_entry.fakeEntry();
  }
};


struct CounterValuesEntry{
  //\event_id/
  int counter_idx;
  uint64_t counter_value;
  uint64_t timestamp;
  int pid;
  int rid;  
  int tid;

  void insert(provDBtables &tables, const std::string &event_id) const{
    auto &tab = tables.counter_values;
    int r = tab.addRow();
    INS(event_id);
    INS(counter_idx);
    INS(counter_value);
    INS(timestamp);
    INS(pid);
    INS(rid);  
    INS(tid);
  }
  void fakeEntry(){
    counter_idx = 1222;
    counter_value = 9890809809;
    timestamp = 89839;
    pid = 0;
    rid = 1;
    tid = 2;
  }
  
};


//For convenience, make each fake event live on a different IO step and attach this entry to the event
struct IOstepsEntry{
  //int <io_step_index>
  int io_step;
  int pid;
  int rid;
  uint64_t io_step_tstart;
  uint64_t io_step_tend;

  void insert(provDBtables &tables, const int io_step_index) const{
    auto &tab = tables.io_steps;
    int r = tab.addRow();
    INS(io_step_index);
    INS(io_step);
    INS(pid);
    INS(rid);
    INS(io_step_tstart);
    INS(io_step_tend);
  }
  void fakeEntry(){    
    io_step = 8789;
    pid = 0;
    rid = 1;
    io_step_tstart = 9879089;
    io_step_tend = 787689999;
  }
  
};




struct AnomaliesEntry{
  std::string event_id;
  uint64_t entry;
  uint64_t exit;
  int fid;
  int pid;
  int rid;
  int tid;
  int io_step_index;
  uint64_t runtime_exclusive;
  double outlier_score;
  double outlier_severity;
  
  std::vector<CallStacksEntry> call_stack;
  std::vector<CounterValuesEntry> counter_events;
  IOstepsEntry io_steps_entry;
  
  void insert(provDBtables &tables) const{
    auto &tab = tables.anomalies;
    int r = tab.addRow();

    INS(event_id);
    INS(entry);
    INS(exit);
    INS(fid);
    INS(pid);
    INS(rid);
    INS(tid);
    INS(io_step_index);
    INS(runtime_exclusive);
    INS(outlier_score);
    INS(outlier_severity);
   
    for(auto const &c: call_stack) c.insert(tables, event_id);
    for(auto const &c: counter_events) c.insert(tables, event_id);
    io_steps_entry.insert(tables, io_step_index);
  }

  void fakeEntry(){
    event_id = uniqueEventID();
    entry = 100;
    exit = 200;
    fid = 1234;
    pid = 0;
    rid = 1;
    tid = 2;
    io_step_index = 99;
    runtime_exclusive = 999;
    outlier_score = 33.331;
    outlier_severity = 999;
    
    call_stack.resize(10);
    for(int i=0;i<call_stack.size();i++) call_stack[i].fakeEntry();

    counter_events.resize(10);
    for(int i=0;i<counter_events.size();i++) counter_events[i].fakeEntry();

    io_steps_entry.fakeEntry();
  }
      
    
};


void test_pdb(duckdb_connection con){
  Timer timer;
  provDBtables tables(con);
  
  AnomaliesEntry e;
  e.fakeEntry();

  e.insert(tables);

  timer.start();
  tables.write(con);
  std::cout << "Write time: " << timer.elapsed_ms() << "ms" << std::endl;
  
  {
    Table rtable_anom("rtable_anom");
    std::stringstream q; q << "SELECT * FROM anomalies WHERE event_id = '" << e.event_id << "'";
    rtable_anom.fromQuery(q.str().c_str(), con);
    std::cout << rtable_anom << std::endl;
  }

  {
    Table rtable_callstack("rtable_callstack");
    std::stringstream q; q << "SELECT * FROM call_stacks WHERE event_key = '" << e.event_id << "'";
    rtable_callstack.fromQuery(q.str().c_str(), con);
    std::cout << rtable_callstack << std::endl;
  }

  {
    Table rtable_callstack_entries("rtable_callstack_entries");
    std::stringstream q; q << "SELECT * FROM call_stack_entry_events";
    rtable_callstack_entries.fromQuery(q.str().c_str(), con);
    std::cout << rtable_callstack_entries << std::endl;
  }

  {
    Table r("r");
    std::stringstream q; q << "SELECT * FROM counter_values WHERE event_id = '" << e.event_id << "'";
    r.fromQuery(q.str().c_str(), con);
    std::cout << r << std::endl;
  }

  {
    Table r("r");
    std::stringstream q; q << "SELECT * FROM io_steps WHERE io_step_index = '" << e.io_step_index << "'";
    r.fromQuery(q.str().c_str(), con);
    std::cout << r << std::endl;
  }

  
}
