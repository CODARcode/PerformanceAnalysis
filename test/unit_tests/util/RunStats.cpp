#include "chimbuko/util/RunStats.hpp"
#include "gtest/gtest.h"
#include <cereal/archives/portable_binary.hpp>
#include <sstream>

using namespace chimbuko;

TEST(TestRunStats, TestStateToFromJSON){
  RunStats stats;
  for(int i=0;i<100;i++) stats.push(i);
  RunStats::State stats_state = stats.get_state();
  
  nlohmann::json json_state = stats_state.get_json();
  
  EXPECT_EQ(stats_state.count, json_state["count"]);
  EXPECT_EQ(stats_state.eta, json_state["eta"]);
  EXPECT_EQ(stats_state.rho, json_state["rho"]);
  EXPECT_EQ(stats_state.tau , json_state["tau"]);
  EXPECT_EQ(stats_state.phi , json_state["phi"]);
  EXPECT_EQ(stats_state.min , json_state["min"]);
  EXPECT_EQ(stats_state.max , json_state["max"]);
  EXPECT_EQ(stats_state.acc , json_state["acc"]);


  RunStats stats2;
  for(int i=0;i<100;i++) stats2.push(2*i);
  RunStats::State stats2_state = stats2.get_state();
  
  json_state["count"] = stats2_state.count;
  json_state["eta"] = stats2_state.eta;
  json_state["rho"] = stats2_state.rho;
  json_state["tau"] = stats2_state.tau;
  json_state["phi"] = stats2_state.phi;
  json_state["min"] = stats2_state.min;
  json_state["max"] = stats2_state.max;
  json_state["acc"] = stats2_state.acc;

  RunStats::State stats_state_tmp;
  stats_state_tmp.set_json(json_state);

  EXPECT_EQ(stats_state_tmp, stats2_state);
}


TEST(TestRunStats, serialize){
  RunStats stats;
  for(int i=0;i<100;i++) stats.push(i);

  std::string ser = stats.net_serialize();
  RunStats stats_rd;
  stats_rd.net_deserialize(ser);
  
  EXPECT_EQ(stats, stats_rd);
}
  

  

  
