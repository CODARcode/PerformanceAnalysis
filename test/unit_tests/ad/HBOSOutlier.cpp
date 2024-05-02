#include<chimbuko/ad/ADOutlier.hpp>
#include<chimbuko/core/param/sstd_param.hpp>
#include<chimbuko/core/param/hbos_param.hpp>
#include<chimbuko/message.hpp>
#include<chimbuko/verbose.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"
#include "unit_test_ad_common.hpp"

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <random>

using namespace chimbuko;

TEST(HBOSADOutlierTestSyncParamWithoutPS, CheckSyncParam){
  chimbuko::enableVerboseLogging() = false;
  
  HbosParam local_params_ps;


  std::default_random_engine gen;
  std::normal_distribution<double> dist(500.,100.), dist2(1000.,200.);

  int N = 50;

  HbosParam local_params_ps_in;
  {
    Histogram &h = local_params_ps_in[0].getHistogram();
    std::vector<double> runtime;
    for(int i=0;i<N;i++) runtime.push_back(dist(gen));
    h.create_histogram(runtime);
    std::cout << "Created Histogram 1" << std::endl;

    runtime.clear();
    for(int i=0;i<100;i++) runtime.push_back(dist2(gen));
    h = Histogram::merge_histograms(h, runtime);
    std::cout << "Merged Histogram" << std::endl;

  }
  local_params_ps.assign(local_params_ps_in);

  std::cout << local_params_ps_in[0].get_json().dump();

  ADOutlierHBOSTest outlier;
  outlier.sync_param_test(&local_params_ps);

  //internal copy should be equal to global copy
  std::string in_state = outlier.get_global_parameters()->serialize();

  EXPECT_EQ(local_params_ps.serialize(), in_state);
}

TEST(HBOSADOutlierTestComputeOutliersWithoutPS, Works){
  //Generate statistics
  std::default_random_engine gen;
  std::normal_distribution<double> dist(420.,10.);
  int N = 50;
  int func_id = 1234;
  HbosParam stats, stats2, stats3, stats4, stats5;
  Histogram &stats_r = stats[func_id].getHistogram(), &stats_r2 = stats2[func_id].getHistogram(), &stats_r3 = stats3[func_id].getHistogram(), &stats_r4 = stats4[func_id].getHistogram(), &stats_r5 = stats5[func_id].getHistogram();
  ADOutlierHBOSTest outlier, outlier2, outlier3, outlier4;

  std::vector<double> runtimes;
  for(int i=0;i<N;i++) runtimes.push_back(dist(gen));

  //Generate the local model
  stats_r.create_histogram(runtimes);
  //Set the global model equal to the local model
  outlier.sync_param_test(&stats);

  //Generate a new local model from data including an explicit outlier
  runtimes.clear();
  std::list<ExecData_t> call_list;  //aka CallList_t
  unsigned long outlier_start, outlier_runtime;
  for(int i=0;i<N;i++){
    double val = i==N-1 ? 800 : double(dist(gen)); //outlier on N-1
    unsigned long start = 1000*(i+1);
    call_list.push_back( createFuncExecData_t(0,0,0,  func_id, "my_func", start,val) );
    runtimes.push_back(val);
    if(i==N-1){
      outlier_start = start;
      outlier_runtime = val;
    }
  }
  stats_r2.create_histogram(runtimes);
  outlier.sync_param_test(&stats2); //merge second data batch into global model
  
  ExecDataMap_t exec_data;
  std::vector<CallListIterator_t> &call_list_its = exec_data[func_id];
  for(CallListIterator_t it=call_list.begin(); it != call_list.end(); ++it)
    call_list_its.push_back(it);

  ADExecDataInterface iface(&exec_data);
  unsigned long nout = outlier.compute_outliers_test(iface, func_id);

  std::cout << "# outliers detected: " << nout << std::endl;
  EXPECT_GE(nout, 1);
  EXPECT_EQ(iface.getDataSetParamIndex(0), func_id);
  //Check the expected outlier is present
  EXPECT_TRUE(outlier.findOutlier(outlier_start, outlier_runtime, 0, iface));

 
  //Generate some events with an outlier itr 2 same outlier val:800. 
  runtimes.clear();
  std::list<ExecData_t> call_list2;  //aka CallList_t
  for(int i=0;i<N;i++){
    double val = i==N-1 ? 800 : double(dist(gen)); //outlier on N-1
    call_list2.push_back( createFuncExecData_t(0,0,0,  func_id, "my_func", 1000*(i+1),val) );
    runtimes.push_back(val);
    //std::cout << call_list.back().get_json().dump() << std::endl;
  }

  stats_r3.create_histogram(runtimes);

  outlier.sync_param_test(&stats3);

  exec_data.clear();
  std::vector<CallListIterator_t> &call_list_its2 = exec_data[func_id];
  for(CallListIterator_t it=call_list2.begin(); it != call_list2.end(); ++it)
    call_list_its2.push_back(it);

  ADExecDataInterface iface2(&exec_data);
  unsigned long nout2 = outlier.compute_outliers_test(iface2, func_id);

  std::cout << "# outliers detected: " << nout2 << std::endl;


  //Generate some events with an outlier itr 3 different outlier val:10000
  runtimes.clear();
  std::list<ExecData_t> call_list3;  //aka CallList_t
  for(int i=0;i<N;i++){
    double val = i==N-1 ? 10000 : double(dist(gen)); //outlier on N-1
    double start = 1000*(i+1);
    call_list3.push_back( createFuncExecData_t(0,0,0,  func_id, "my_func", start,val) );
    runtimes.push_back(val);
    if(i==N-1){
      outlier_start = start;
      outlier_runtime = val;
    }
  }

  stats_r4.create_histogram(runtimes);
  outlier.sync_param_test(&stats4);

  exec_data.clear();
  std::vector<CallListIterator_t> &call_list_its3 = exec_data[func_id];
  for(CallListIterator_t it=call_list3.begin(); it != call_list3.end(); ++it)
    call_list_its3.push_back(it);

  ADExecDataInterface iface3(&exec_data); 
  enableVerboseLogging() = true;
  unsigned long nout3 = outlier.compute_outliers_test(iface3, func_id);
  enableVerboseLogging() = false;

  std::cout << "# outliers detected: " << nout3 << std::endl;
  EXPECT_GE(nout, 1);
  //Check the expected outlier is present
  outlier.printOutliers(iface3);
  std::cout << "EXPECT " << outlier_start << " " << outlier_runtime << std::endl;

  EXPECT_TRUE(outlier.findOutlier(outlier_start, outlier_runtime, 0, iface3));
}


TEST(HBOSADOutlierTestSyncParamWithPS, Works){
  HbosParam global_params_ps; //parameters held in the parameter server
  HbosParam local_params_ad; //parameters collected by AD

  std::default_random_engine gen;
  std::normal_distribution<double> dist(500.,100.);
  int N = 50;

  {
    std::vector<double> runtimes;
    Histogram &r = global_params_ps[0].getHistogram();
    for(int i=0;i<N;i++) runtimes.push_back(dist(gen));
    r.create_histogram(runtimes);
  }

  {
    std::vector<double> runtimes;
    Histogram &r = local_params_ad[0].getHistogram();
    for(int i=0;i<N;i++) runtimes.push_back(dist(gen));
    r.create_histogram(runtimes);
  }

  std::cout << global_params_ps[0].get_json().dump();
  std::cout << local_params_ad[0].get_json().dump();

  HbosParam combined_params_ps; //what we expect
  combined_params_ps.assign(global_params_ps);
  combined_params_ps.update(local_params_ad);


#ifdef _USE_MPINET
#warning "Testing with MPINET not available"
#elif defined(_USE_ZMQNET)
  std::cout << "Using ZMQ net" << std::endl;

  Barrier barrier2(2);

  std::string sinterface = "tcp://*:5559";
  std::string sname = "tcp://localhost:5559";
  ZMQNet ps;
  int argc; char** argv = nullptr;
  std::cout << "Initializing PS thread" << std::endl;
  std::thread ps_thr([&]{
      int nt = 4; //4 workers
      for(int i=0;i<nt;i++){
	ps.add_payload(new NetPayloadUpdateParams(&global_params_ps),i);
	ps.add_payload(new NetPayloadGetParams(&global_params_ps),i);
      }
      ps.init(&argc, &argv, nt);
      ps.run(".");
      std::cout << "PS thread waiting at barrier" << std::endl;
      barrier2.wait();
      std::cout << "PS thread terminating connection" << std::endl;
      ps.finalize();
    });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::string glob_params_comb_ad;
  std::cout << "Initializing AD thread" << std::endl;
  std::thread out_thr([&]{
			try{
			  ADThreadNetClient net_client;
			  net_client.connect_ps(0, 0, sname);
			  ADOutlierHBOSTest outlier;
			  outlier.linkNetworkClient(&net_client);
			  outlier.sync_param_test(&local_params_ad); //add local to global in PS and return to AD
			  glob_params_comb_ad  = outlier.get_global_parameters()->serialize();

			  std::cout << "AD thread terminating connection" << std::endl;
			  net_client.disconnect_ps();
			  std::cout << "AD thread waiting at barrier" << std::endl;
			  barrier2.wait();
			}catch(const std::exception &e){
			  std::cerr << e.what() << std::endl;
			}

		      });

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  std::thread stop_ps([&]{ try{ps.stop();}catch(const std::exception &e) {std::cerr << e.what() << std::endl;}});
  stop_ps.join();
  ps_thr.join();
  out_thr.join();



#else
#error "Requires compiling with MPI or ZMQ net"
#endif
}

// TEST(HBOSADOutlierTest, TestFunctionThresholdOverride){
//   int func_id =101;
//   int func_id2 = 202;
//   double default_threshold = 0.99;
//   ADOutlierHBOS ad(default_threshold);
//   ad.overrideFuncThreshold("my_func",0.77);
//   EXPECT_EQ(ad.getFunctionThreshold("my_func"), 0.77);
//   EXPECT_EQ(ad.getFunctionThreshold("my_other_func"), default_threshold);
// }
