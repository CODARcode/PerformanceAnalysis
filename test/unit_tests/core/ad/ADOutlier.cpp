#include<chimbuko/core/ad/ADOutlier.hpp>
#include <fstream>
#include "gtest/gtest.h"

using namespace chimbuko;

TEST(ADOutlier, serializeAlgoParams){
  ADOutlier::AlgoParams pin;
  pin.algorithm = "copod";
  pin.sstd_sigma = 3.14;
  pin.hbos_thres = 0.33;
  pin.glob_thres = false;
  pin.hbos_max_bins = 1234;

  nlohmann::json j = pin.getJson();
  
  ADOutlier::AlgoParams pout;
  pout.setJson(j);

  EXPECT_EQ(pin,pout);

  std::string fn = "/tmp/algo_params.json";
  {
    std::ofstream f(fn);
    f << j.dump(4);
  }
  ADOutlier::AlgoParams pfile;
  pfile.loadJsonFile(fn);

  EXPECT_EQ(pin,pfile);
}
