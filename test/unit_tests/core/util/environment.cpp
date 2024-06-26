#include <sstream>
#include "chimbuko/core/util/environment.hpp"
#include "gtest/gtest.h"
#include "../../unit_test_cmdline.hpp"

using namespace chimbuko;


TEST(TestUtils, testGetHostname){
  std::string expect = "";
  for(int i=1;i<_argc-1;i++){
    if(std::string(_argv[i]) == "-hostname"){
      expect = _argv[i+1];
      break;
    }
  }
  if(expect == ""){
    throw std::runtime_error("Test must be run with '-hostname ${hostname}' passed on the command line");
  }
  std::string got = getHostname();
  std::cout << "Got: " << got << " expect: " << expect << std::endl;
  
  EXPECT_EQ(got, expect);

  //Test successive calls work and point to the same string
  got = getHostname();
  EXPECT_EQ(got, expect);

  std::string const* p1 = &getHostname();
  std::string const* p2 = &getHostname();
  EXPECT_EQ(p1,p2);

}
