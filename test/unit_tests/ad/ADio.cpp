#include<chimbuko/ad/ADio.hpp>
#include "gtest/gtest.h"
#include <experimental/filesystem>

using namespace chimbuko;

TEST(ADioTestsetOutputPath, createsDirectory){
  ADio io;
  io.setDestructorThreadWaitTime(0); //remove annoying wait
  const static std::string path = "test_dir_to_delete";
  io.setOutputPath(path);

  EXPECT_EQ( std::experimental::filesystem::is_directory(path), true );
  std::experimental::filesystem::remove(path);
}

TEST(ADioTestwrite, OKifnoCurlOrpath){ //no curl or path set
  ADio io;
  io.setDestructorThreadWaitTime(0); //remove annoying wait

  CallListMap_p_t* callListMap = new CallListMap_p_t;

  EXPECT_EQ( io.write(callListMap, 1), IOError::OK );
}
TEST(ADioTestwrite, OKifPathNoDispatch){ //path set, no curl or thread dispatch
  ADio io;
  io.setDestructorThreadWaitTime(0); //remove annoying wait

  const static std::string path = "test_dir_to_delete2";
  io.setOutputPath(path);
  io.setRank(1234);
  CallListMap_p_t* callListMap = new CallListMap_p_t;

  ExecData_t event;
  event.set_label(-1); //mark as anomaly
  
  (*callListMap)[0][0][0].push_back(event);
  
  EXPECT_EQ( io.write(callListMap, 5678), IOError::OK );

  //Check the file was created
  EXPECT_EQ( std::experimental::filesystem::is_regular_file(path + "/0/1234/5678.json"), true );
  
  std::experimental::filesystem::remove_all(path);
}
