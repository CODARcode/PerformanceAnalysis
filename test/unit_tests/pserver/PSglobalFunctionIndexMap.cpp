#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include <chimbuko/pserver/PSglobalFunctionIndexMap.hpp>

using namespace chimbuko;

TEST(TestPSglobalFunctionIndexMap, CheckLookup){  
  PSglobalFunctionIndexMap pm;
  EXPECT_EQ( pm.lookup("hello_world"), 0 );
  EXPECT_EQ( pm.lookup("hello_again"), 1 );
  EXPECT_EQ( pm.lookup("hello_world"), 0 );
  EXPECT_EQ( pm.lookup("hello_again"), 1 );
}



TEST(TestPSglobalFunctionIndexMap, CheckSerialization){  
  PSglobalFunctionIndexMap pm;
  pm.lookup("hello_world");
  pm.lookup("hello_again");
  
  nlohmann::json ser = pm.serialize();
  
  PSglobalFunctionIndexMap pmr;
  pmr.deserialize(ser);
  EXPECT_EQ( pmr.contains("hello_world"), true );
  EXPECT_EQ( pmr.contains("hello_again"), true );
  EXPECT_EQ( pmr.lookup("another_func"), 2);
}

