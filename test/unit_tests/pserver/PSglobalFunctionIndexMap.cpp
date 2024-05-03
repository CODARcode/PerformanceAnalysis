#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include <chimbuko/modules/performance_analysis/pserver/PSglobalFunctionIndexMap.hpp>

using namespace chimbuko;

TEST(TestPSglobalFunctionIndexMap, CheckLookup){  
  PSglobalFunctionIndexMap pm;
  EXPECT_EQ( pm.lookup(0, "hello_world"), 0 );
  EXPECT_EQ( pm.lookup(0, "hello_again"), 1 );
  EXPECT_EQ( pm.lookup(0, "hello_world"), 0 );
  EXPECT_EQ( pm.lookup(0, "hello_again"), 1 );

  //Test that same function names but with different program indices are assigned different global indices
  EXPECT_EQ( pm.lookup(1, "hello_world"), 2 );
}



TEST(TestPSglobalFunctionIndexMap, CheckSerialization){  
  PSglobalFunctionIndexMap pm;
  pm.lookup(0, "hello_world");
  pm.lookup(0, "hello_again");
  
  nlohmann::json ser = pm.serialize();
  
  PSglobalFunctionIndexMap pmr;
  pmr.deserialize(ser);
  EXPECT_EQ( pmr.contains(0, "hello_world"), true );
  EXPECT_EQ( pmr.contains(0, "hello_again"), true );
  EXPECT_EQ( pmr.lookup(0, "another_func"), 2);
}

