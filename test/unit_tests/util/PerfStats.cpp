#include <sstream>
#include <cstdio>
#include "chimbuko/util/PerfStats.hpp"
#include "gtest/gtest.h"

using namespace chimbuko;

bool file_exists(const std::string &fileName){
  std::ifstream infile(fileName);
  return infile.good();
}

TEST(TestPerfPeriodic, runTests){
#ifdef _PERF_METRIC
  std::string filename = "perfperiodic_test";
  std::cout << "Running PerfPeriodic tests" << std::endl;
  PerfPeriodic perf(".",filename);

  //Test nothing written if no data
  remove(filename.c_str());
  
  perf.write();

  EXPECT_EQ( file_exists(filename), false );

  perf.add("anumber",1234);
  perf.add("hello","world");

  perf.write();

  std::string date,time,v1,v2;
  {
    std::ifstream in(filename);
    EXPECT_EQ(in.good(),true);
    in >> date >> time >> v1 >> v2;
    EXPECT_EQ(in.fail(),false);

    //unordered_map decides the order
    bool comb1 = v1 == "anumber:1234" && v2 == "hello:world";
    bool comb2 = v2 == "anumber:1234" && v1 == "hello:world";

    EXPECT_EQ(comb1 || comb2, true);
  }
  //Writing should have flushed

  perf.add("howdy", "stranger");
  perf.write();
  
  {
    std::ifstream in(filename);
    EXPECT_EQ(in.good(),true);
    in >> date >> time >> v1 >> v2;
    in >> date >> time >> v1;
    EXPECT_EQ(in.fail(),false);

    //unordered_map decides the order
    EXPECT_EQ(v1, "howdy:stranger");
  }




#endif
}
