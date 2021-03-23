#include "chimbuko/util/RunStats.hpp"
#include "gtest/gtest.h"
#include <cereal/archives/portable_binary.hpp>
#include <sstream>

using namespace chimbuko;

TEST(TestRunStats, serialize){
  RunStats stats;
  for(int i=0;i<100;i++) stats.push(i);
  RunStats::State stats_state = stats.get_state();

  std::string str_state;
  {
    std::stringstream ss;  
    {
      cereal::PortableBinaryOutputArchive wr(ss);
      wr(stats_state);    
    }
    str_state = ss.str();
  }
  std::cout << str_state.size() << " / " << stats.get_json_state().dump().size() << std::endl;

  RunStats::State stats_state_rd;
  {
    std::stringstream ss;
    ss << str_state;
    {
      cereal::PortableBinaryInputArchive rd(ss);
      rd(stats_state_rd);    
    }
  }
  
  EXPECT_EQ(stats_state, stats_state_rd);
}
  

  

  
