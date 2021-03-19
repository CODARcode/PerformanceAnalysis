#include<chimbuko/ad/ADio.hpp>
#include "gtest/gtest.h"
#include <experimental/filesystem>

using namespace chimbuko;

TEST(ADioTestsetOutputPath, createsDirectory){
  ADio io(0,0);
  io.setDestructorThreadWaitTime(0); //remove annoying wait
  const static std::string path = "test_dir_to_delete";
  io.setOutputPath(path);

  EXPECT_EQ( std::experimental::filesystem::is_directory(path), true );
  std::experimental::filesystem::remove(path);
}

TEST(ADioTestwrite, OKifnoPath){ //no path set
  ADio io(0,0);
  io.setDestructorThreadWaitTime(0); //remove annoying wait
  
  nlohmann::json test = { {"hello","world"} };
  std::vector<nlohmann::json> test_j(5,test);

  EXPECT_EQ( io.writeJSON(test_j, 1, "test"), IOError::OK );
}
TEST(ADioTestwrite, OKifPathNoDispatch){ //path set, no curl or thread dispatch
  ADio io(2233,1234);
  io.setDestructorThreadWaitTime(0); //remove annoying wait

  const static std::string path = "test_dir_to_delete2";
  io.setOutputPath(path);

  nlohmann::json test = { {"hello","world"} };
  std::vector<nlohmann::json> test_j(5,test);

  EXPECT_EQ( io.writeJSON(test_j, 5678, "test"), IOError::OK );

  //Check the file was created
  EXPECT_EQ( std::experimental::filesystem::is_regular_file(path + "/2233/1234/5678.test.json"), true );
  
  std::experimental::filesystem::remove_all(path);
}
