#include <random>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include <chimbuko/pserver/AggregateAnomalyData.hpp>

using namespace chimbuko;


TEST(TestAggregateAnomalyData, Combine){  
  AnomalyData anom1(11, 12, 13, 14, 15, 16);
  for(int i=0;i<16;i++) anom1.add_outlier_score(M_PI*(i+1));

  AnomalyData anom2(17, 18, 19, 20, 21, 22);
  for(int i=0;i<22;i++) anom2.add_outlier_score(M_PI*M_PI*(i+3));
  
  AggregateAnomalyData agg_all;
  agg_all.add(anom1);
  agg_all.add(anom2);

  ASSERT_EQ(agg_all.get_data()->size(),2);
  EXPECT_EQ(agg_all.get_data()->front(), anom1);
  EXPECT_EQ(agg_all.get_data()->back(), anom2);
  
  AggregateAnomalyData agg1;
  agg1.add(anom1);

  AggregateAnomalyData agg2;
  agg2.add(anom2);

  //Combine in reverse order for test purposes
  AggregateAnomalyData comb(agg2);
  comb += agg1;

  ASSERT_EQ(comb.get_data()->size(),2);
  EXPECT_EQ(comb.get_data()->front(), anom2);
  EXPECT_EQ(comb.get_data()->back(), anom1);
  
  RunStats rcomb = comb.get_stats();
  RunStats rall = agg_all.get_stats();
  
  EXPECT_NEAR(rcomb.mean(), rall.mean(), 1e-8);
  EXPECT_NEAR(rcomb.stddev(), rall.stddev(), 1e-8);
}
