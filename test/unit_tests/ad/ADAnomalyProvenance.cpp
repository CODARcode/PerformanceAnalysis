#include<chimbuko/ad/ADAnomalyProvenance.hpp>
#include<chimbuko/param/sstd_param.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>

using namespace chimbuko;

TEST(TestADAnomalyProvenance, extractsCallInformation){
  ExecData_t exec1 = createFuncExecData_t(1,2,3, 55, "theparent", 1000, 100);
  ExecData_t exec2 = createFuncExecData_t(1,2,3, 77, "thechild", 1050, 50); //going to be the anomalous one
  exec2.set_parent(exec1.get_id());
  exec1.inc_n_children();
  exec1.update_exclusive(exec2.get_runtime());

  ADEvent event_man;
  event_man.addCall(exec1);
  event_man.addCall(exec2);

  RunStats stats;
  for(int i=0;i<50;i++)
    stats.push(double(i));

  SstdParam param;
  param[77] = stats;

  ADCounter counter;
  ADMetadataParser metadata;

  ADAnomalyProvenance prov(exec2,
			   event_man,
			   param,
			   counter, metadata);
    
  nlohmann::json output = prov.get_json();
  //Test pid,rid,tid,runtime
  EXPECT_EQ(output["pid"], 1);
  EXPECT_EQ(output["rid"], 2);
  EXPECT_EQ(output["tid"], 3);
  EXPECT_EQ(output["runtime_total"], 50);
  
  //Check call stack
  EXPECT_EQ(output["call_stack"].size(), 2);
  EXPECT_EQ(output["call_stack"][0]["func"], "thechild");
  EXPECT_EQ(output["call_stack"][1]["func"], "theparent");

  //Test statistics
  EXPECT_EQ(output["func_stats"], stats.get_json());
  
}
