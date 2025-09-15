#include "chimbuko/core/param/copod_param.hpp"
#include "gtest/gtest.h"
#include <sstream>
#include <limits>
#include <random>

using namespace chimbuko;

TEST(TestCopodParam, serialize){
  std::default_random_engine gen(1234);
  std::normal_distribution<double> dist(50.,10.);
  int N = 50;
  int fid = 123;
  double init_global_threshold = 100;
  int maxbins = 200;

  std::vector<double> runtimes(N);
  for(int i=0;i<N;i++){
    runtimes[i] = dist(gen);
  }

  CopodParam l;
  l[fid].getHistogram() = Histogram(runtimes);
  l[fid].setInternalGlobalThreshold(1234);

  std::string ser = l.serialize();
  
  CopodParam r;
  r.assign(ser);

  EXPECT_EQ(l.get_copodstats(),r.get_copodstats());
}

TEST(TestCopodParam, serializeJSON){
  std::default_random_engine gen(1234);
  std::normal_distribution<double> dist(50.,10.);
  int N = 50;
  int fid = 123;
  double init_global_threshold = 100;
  int maxbins = 200;

  std::vector<double> runtimes(N);
  for(int i=0;i<N;i++){
    runtimes[i] = dist(gen);
  }

  CopodParam l;
  l[fid].getHistogram() = Histogram(runtimes);
  l[fid].setInternalGlobalThreshold(1234);

  nlohmann::json ser = l.serialize_json();
  
  CopodParam r;
  r.deserialize_json(ser);

  EXPECT_EQ(l.get_copodstats(),r.get_copodstats());
}
