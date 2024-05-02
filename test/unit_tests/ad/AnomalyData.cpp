#include<chimbuko/ad/AnomalyData.hpp>
#include<chimbuko/core/util/error.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"


using namespace chimbuko;


TEST(AnomalyDataTest, TestSerialization){
  int pid = 0, rid=1, step=2,  min_ts=500,  max_ts=600,  n_anom = 1234;  
  AnomalyData d(pid, rid, step, min_ts, max_ts, n_anom);
  for(int i=0;i<100;i++) d.add_outlier_score(i);

  std::string ds = d.net_serialize();
  
  AnomalyData c;
  c.net_deserialize(ds);
  
  EXPECT_EQ(d,c);
}
