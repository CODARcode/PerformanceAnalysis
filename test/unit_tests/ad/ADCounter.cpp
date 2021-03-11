#include<chimbuko/ad/ADCounter.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

using namespace chimbuko;


TEST(ADCounterTest, CounterAddedCorrectly){
  ADCounter counters;
  
  unsigned long 
    pid = 0,
    rid = 1,
    tid = 2,
    id = 99,
    value = 1234,
    ts = 5678;
  
  Event_t ev = createCounterEvent_t(pid, rid, tid, id, value, ts);
  std::unordered_map<int, std::string> cmap;
  cmap[id] = "MyCounter";
  counters.linkCounterMap(&cmap);
  counters.addCounter(ev);
  
  //Check counter has been stored
  CounterDataListMap_p_t const* cprt = counters.getCounters(); //p/r/t -> std::list<CounterData_t>
  auto const &pmap = *cprt;
  EXPECT_EQ(pmap.size(), 1);
  EXPECT_EQ(pmap.find(pid), pmap.begin());
  auto const &rmap = pmap.begin()->second;
  EXPECT_EQ(rmap.size(), 1);
  EXPECT_EQ(rmap.find(rid), rmap.begin());
  auto const &tmap = rmap.begin()->second;
  EXPECT_EQ(tmap.size(), 1);
  EXPECT_EQ(tmap.find(tid), tmap.begin());
  const std::list<CounterData_t> &clist = tmap.begin()->second;
  EXPECT_EQ(clist.size(), 1);
  const CounterData_t &cval = *clist.begin();

  EXPECT_EQ(cval.get_pid(), pid);
  EXPECT_EQ(cval.get_rid(), rid);
  EXPECT_EQ(cval.get_tid(), tid);
  EXPECT_EQ(cval.get_counterid(), id);
  EXPECT_EQ(cval.get_countername(), "MyCounter");
  EXPECT_EQ(cval.get_value(), value);
  EXPECT_EQ(cval.get_ts(), ts);

  //Check the counter appears in the window
  std::list<CounterDataListIterator_t> window = counters.getCountersInWindow(pid, rid, tid, 5000, 6000);
  EXPECT_EQ(window.size(), 1);
  EXPECT_EQ(*window.begin(), clist.begin());

  window = counters.getCountersInWindow(pid, rid, tid, 6000, 7000);
  EXPECT_EQ(window.size(), 0);
  
  //Check the counter appears in the map by index
  const CountersByIndex_t &cbyidx = counters.getCountersByIndex();
  EXPECT_EQ(cbyidx.size(), 1);
  EXPECT_EQ(cbyidx.find(id), cbyidx.begin());
  const std::list<CounterDataListIterator_t> &idctrs = cbyidx.begin()->second;
  EXPECT_EQ(idctrs.size(), 1);
  EXPECT_EQ(*idctrs.begin(), clist.begin());

  //Try adding a counter event not in the counter list and ensure it throws an error
  Event_t ev2 = createCounterEvent_t(pid, rid, tid, 101, value, ts);
  bool caught_error = false;
  try{
    counters.addCounter(ev2);
  }catch(const std::exception &e){
    std::cout << "Caught expected exception: " << e.what() << std::endl;
    caught_error = true;
  }
  EXPECT_TRUE(caught_error);

}
