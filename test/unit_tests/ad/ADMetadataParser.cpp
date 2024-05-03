#include<chimbuko/modules/performance_analysis/ad/ADMetadataParser.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

using namespace chimbuko;


TEST(ADMetadataParser, ParseCudaDeviceAndContext){
  ADMetadataParser parser;
  std::vector<MetaData_t> md = {  MetaData_t(0,0, 9, "CUDA Context", "1"), MetaData_t(0,0, 9, "CUDA Device", "2"), MetaData_t(0,0, 9, "CUDA Stream", "3") };
  parser.addData(md);
  
  EXPECT_EQ( parser.isGPUthread(9), true );
  const auto &thr_map = parser.getGPUthreadMap();
  
  auto it = thr_map.find(9);
  EXPECT_NE( it, thr_map.end() );

  EXPECT_EQ( it->second.context, 1); //context
  EXPECT_EQ( it->second.device, 2); //device
  EXPECT_EQ( it->second.stream, 3); //stream
}

TEST(ADMetadataParser, ParseGPUproperties){
  ADMetadataParser parser;
  std::vector<MetaData_t> md = {  MetaData_t(0,0, 1234, "GPU[9] Clock Rate", "98765"), MetaData_t(0,0, 1234, "GPU[9] Name", "NVidia Deathstar") };
  parser.addData(md);

  const auto & gpu_props = parser.getGPUproperties();
  
  auto it = gpu_props.find(9);
  EXPECT_NE( it, gpu_props.end() );

  const auto & dev_props = it->second;
  
  EXPECT_EQ( dev_props.size(), 2);
  auto dit = dev_props.find("Clock Rate");
  EXPECT_NE( dit, dev_props.end() );
  EXPECT_EQ( dit->second, "98765");
  
  dit = dev_props.find("Name");
  EXPECT_NE( dit, dev_props.end() );
  EXPECT_EQ( dit->second, "NVidia Deathstar");
}

TEST(ADMetadataParser, ParseHostname){
  ADMetadataParser parser;
  EXPECT_EQ( parser.getHostname(), "Unknown");

  std::vector<MetaData_t> md = {  MetaData_t(0,0, 9, "Hostname", "TheHost") };
  parser.addData(md);
  
  EXPECT_EQ( parser.getHostname(), "TheHost" );
}
