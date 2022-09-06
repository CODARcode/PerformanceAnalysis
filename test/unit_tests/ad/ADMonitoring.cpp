#include<chimbuko/ad/ADMonitoring.hpp>
#include<chimbuko/util/error.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"
#include <fstream>

using namespace chimbuko;

CountersByIndex_t getIndexCounterMap(std::list<CounterData_t> &clist){
  CountersByIndex_t out;
  for(auto it = clist.begin(); it != clist.end(); ++it)
    out[it->get_counterid()].push_back(it);
  return out;
}

TEST(ADMonitoringTest, Basics){
  std::unordered_map<int, std::string> counter_map = { {0,"uninteresting counter"}, {1,"interesting counter"} };
  ADMonitoring mon;

  EXPECT_FALSE(mon.stateIsSet());
  EXPECT_TRUE( mon.get_json().empty() );
  
  std::list<CounterData_t> clist = {
    createCounterData_t(0,0,1,0,1233,9999,"uninteresting counter"), //pid,rid,tid,cid,val,ts,cname
    createCounterData_t(0,0,0,1,9876,10000,"interesting counter") };

  std::ostringstream os;
  Error().setStream(&os);
  
  //Check recoverable error if we haven't linked the counter map
  mon.extractCounters(getIndexCounterMap(clist));
  EXPECT_TRUE( os.str().find("Error (recoverable)") != std::string::npos );
  os.str("");

  mon.linkCounterMap(&counter_map);

  //Add one of the counters to the watchlist
  mon.addWatchedCounter("interesting counter", "my counter field 1");
  mon.extractCounters(getIndexCounterMap(clist));

  ASSERT_TRUE( mon.stateIsSet() );
  ASSERT_TRUE( mon.hasState("my counter field 1") );
  ASSERT_FALSE( mon.hasState("my invalid counter field") );

  auto const & e1 = mon.getState("my counter field 1");
  EXPECT_EQ( e1.field_name, "my counter field 1");
  EXPECT_EQ( e1.value, 9876);
  EXPECT_EQ( mon.getTimeStamp(), 10000 );
  
  //Check we get a recoverable error if the thread is not 0
  clist = {
    createCounterData_t(0,0,33,1,11334,10006,"interesting counter") };

  mon.extractCounters(getIndexCounterMap(clist));
  EXPECT_TRUE( os.str().find("Error (recoverable)") != std::string::npos );
  os.str("");

  //JSON format; should only show latest state
  clist = {
    createCounterData_t(0,0,0,1,555,10010,"interesting counter") };

  mon.extractCounters(getIndexCounterMap(clist));
  EXPECT_EQ( mon.getTimeStamp(), 10010 );

  nlohmann::json j = mon.get_json();
  
  std::cout << j.dump(4) << std::endl;

  ASSERT_TRUE( j.is_object() );
  EXPECT_EQ( j["timestamp"], 10010 );
  
  ASSERT_TRUE( j["data"].is_array() );
  ASSERT_EQ( j["data"].size(), 1 );
  EXPECT_EQ( j["data"][0]["field"], "my counter field 1" );
  EXPECT_EQ( j["data"][0]["value"], 555 );

  //Check we don't output state for a valid counter that has not yet been assigned a value
  counter_map[2] = "another interesting counter";
  mon.addWatchedCounter("another interesting counter", "test field");
  clist = {};
  mon.extractCounters(getIndexCounterMap(clist));
  //  should be an entry for this counter
  try{
    mon.getState("test field");
  }catch(const std::exception &e){
    std::string es = e.what();
    ASSERT_NE( es.find("State entry exists but has not yet been assigned"), std::string::npos );
  }

}

TEST(ADMonitoringTest, parseUserInputs){
  ADMonitoring mon;
  mon.setDefaultWatchList(); //so we can check the list is flushed prior to reading

  {
    std::ofstream f("mon.json");
    f << "[ [\"hello\",\"world\"] ]" << std::endl;
  }
  mon.parseWatchListFile("mon.json");
  auto const & w = mon.getWatchList();

  ASSERT_EQ( w.size(), 1 );
  auto it = w.begin();
  EXPECT_EQ(it->first, "hello");
  EXPECT_EQ(it->second, "world");
}
