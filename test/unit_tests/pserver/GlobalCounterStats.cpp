#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include <chimbuko/pserver/GlobalCounterStats.hpp>
#include <chimbuko/util/string.hpp>
#include<random>

using namespace chimbuko;

bool near(const RunStats &a, const RunStats &b, double tol){
  return fabs( a.mean() - b.mean() ) < tol && fabs( a.stddev() - b.stddev() ) < tol;
}

TEST(TestGlobalCounterStats, Combination){
  int pid = 0;
  int cid1 = 99;
  int cid2 = 33;
  int step1 = 11;
  int step2 = 66;
  std::string cname1 = "counter1";
  std::string cname2 = "counter2";
  std::unordered_set<std::string> which_counters = {cname1,cname2};

  RunStats stats1, stats2;
  for(int i=0;i<100;i++){
    stats1.push(M_PI*(i-1) + M_PI*i*i);
    stats2.push(M_PI*M_PI*(i+3) + M_PI*M_PI*M_PI*i*i*i);
  }

  ADLocalCounterStatistics::State::CounterData cstate1;
  cstate1.pid = pid;
  cstate1.name = cname1;
  cstate1.stats = stats1.get_state();

  ADLocalCounterStatistics::State::CounterData cstate2;
  cstate2.pid = pid;
  cstate2.name = cname2;
  cstate2.stats = stats2.get_state();

  ADLocalCounterStatistics::State state_counter1_1;
  state_counter1_1.step = step1;
  state_counter1_1.counters = {cstate1};

  ADLocalCounterStatistics data_counter1_1(pid, step1, &which_counters);
  data_counter1_1.set_state(state_counter1_1);

  ADLocalCounterStatistics::State state_counter1_2;
  state_counter1_2.step = step2;
  state_counter1_2.counters = {cstate2};

  ADLocalCounterStatistics data_counter1_2(pid, step2, &which_counters);
  data_counter1_2.set_state(state_counter1_2);

  ADLocalCounterStatistics::State state_counter2_1;
  state_counter2_1.step = step1;
  state_counter2_1.counters = {cstate2};

  ADLocalCounterStatistics data_counter2_1(pid, step1, &which_counters);
  data_counter2_1.set_state(state_counter2_1);
  
  {
    //Check merge for stats for 2 different counters
    GlobalCounterStats glb_all;
    glb_all.add_counter_data(data_counter1_1);
    glb_all.add_counter_data(data_counter2_1);
    auto glb_all_stats = glb_all.get_stats();
    auto pit = glb_all_stats.find(pid);
    ASSERT_NE(pit, glb_all_stats.end());
    auto all_cit1 = pit->second.find(cname1);
    ASSERT_NE(all_cit1, pit->second.end());
    auto all_cit2 = pit->second.find(cname2);
    ASSERT_NE(all_cit2, pit->second.end());

    GlobalCounterStats glb_1;
    glb_1.add_counter_data(data_counter1_1);

    GlobalCounterStats glb_2;
    glb_2.add_counter_data(data_counter2_1);

    GlobalCounterStats glb_comb(glb_1);
    glb_comb += glb_2;

    auto glb_comb_stats = glb_comb.get_stats();
    pit = glb_comb_stats.find(pid);
    ASSERT_NE(pit, glb_comb_stats.end());
    auto comb_cit1 = pit->second.find(cname1);
    ASSERT_NE(comb_cit1, pit->second.end());
    auto comb_cit2 = pit->second.find(cname2);
    ASSERT_NE(comb_cit2, pit->second.end());
    
    ASSERT_EQ( near(comb_cit1->second, all_cit1->second, 1e-8), true );
    ASSERT_EQ( near(comb_cit2->second, all_cit2->second, 1e-8), true );
  }

  {
    //Check merge for stats for the same counter for 2 data points
    GlobalCounterStats glb_all;
    glb_all.add_counter_data(data_counter1_1);
    glb_all.add_counter_data(data_counter1_2);
    auto glb_all_stats = glb_all.get_stats();
    auto pit = glb_all_stats.find(pid);
    ASSERT_NE(pit, glb_all_stats.end());
    auto all_cit1 = pit->second.find(cname1);
    ASSERT_NE(all_cit1, pit->second.end());

    GlobalCounterStats glb_1;
    glb_1.add_counter_data(data_counter1_1);

    GlobalCounterStats glb_2;
    glb_2.add_counter_data(data_counter1_2);

    GlobalCounterStats glb_comb(glb_1);
    glb_comb += glb_2;

    auto glb_comb_stats = glb_comb.get_stats();
    pit = glb_comb_stats.find(pid);
    ASSERT_NE(pit, glb_comb_stats.end());
    auto comb_cit1 = pit->second.find(cname1);
    ASSERT_NE(comb_cit1, pit->second.end());
    
    ASSERT_EQ( near(comb_cit1->second, all_cit1->second, 1e-8), true );
  }   

}
