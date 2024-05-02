#include<chimbuko/ad/ADOutlier.hpp>
#include<chimbuko/core/param/sstd_param.hpp>
#include<chimbuko/core/param/hbos_param.hpp>
#include<chimbuko/core/param/copod_param.hpp>
#include<chimbuko/message.hpp>
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

TEST(COPODADOutlierTestSyncParamWithoutPS, Works){
  CopodParam local_params_ps;

  std::default_random_engine gen;
  std::normal_distribution<double> dist(500.,100.), dist2(1000.,200.);

  int N = 50;

  CopodParam local_params_ps_in;
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

  ADOutlierCOPODTest outlier;
  outlier.sync_param_test(&local_params_ps);

  //internal copy should be equal to global copy
  std::string in_state = outlier.get_global_parameters()->serialize();

  EXPECT_EQ(local_params_ps.serialize(), in_state);
}

TEST(COPODADOutlierTestComputeOutliersWithoutPS, Works){
  //Generate statistics
  std::default_random_engine gen;
  std::normal_distribution<double> dist(420.,10.);
  int N = 50;
  int func_id = 1234;
  CopodParam stats, stats2, stats3, stats4, stats5;
  Histogram &stats_r = stats[func_id].getHistogram(), &stats_r2 = stats2[func_id].getHistogram(), &stats_r3 = stats3[func_id].getHistogram(), &stats_r4 = stats4[func_id].getHistogram(), &stats_r5 = stats5[func_id].getHistogram();
  std::vector<double> runtimes;
  for(int i=0;i<N;i++) runtimes.push_back(dist(gen));
  stats_r.create_histogram(runtimes);

  ADOutlierCOPODTest outlier, outlier2, outlier3, outlier4;

  outlier.sync_param_test(&stats);

  std::string stats_state = outlier.get_global_parameters()->serialize();

  std::cout << "Stats: " << stats_state << std::endl;

  //Generate some events with an outlier itr 1
  runtimes.clear();
  std::list<ExecData_t> call_list;  //aka CallList_t
  for(int i=0;i<N;i++){
    double val = i==N-1 ? 800 : double(dist(gen)); //outlier on N-1
    call_list.push_back( createFuncExecData_t(0,0,0,  func_id, "my_func", 1000*(i+1),val) );
    runtimes.push_back(val);
    //std::cout << call_list.back().get_json().dump() << std::endl;
  }

  stats_r2.create_histogram(runtimes);

  outlier.sync_param_test(&stats2);

  std::string stats_state2 = outlier.get_global_parameters()->serialize();

  std::cout << "Stats: " << stats_state2 << std::endl;

  std::vector<CallListIterator_t> call_list_its;
  for(CallListIterator_t it=call_list.begin(); it != call_list.end(); ++it)
    call_list_its.push_back(it);

  unsigned long nout = outlier.compute_outliers_test(func_id, call_list_its);

  std::cout << "# outliers detected: " << nout << std::endl;

  //Generate some events with an outlier itr 2 same outlier val:800
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

  std::string stats_state3 = outlier.get_global_parameters()->serialize();

  std::cout << "Stats: " << stats_state3 << std::endl;

  std::vector<CallListIterator_t> call_list_its2;
  for(CallListIterator_t it=call_list2.begin(); it != call_list2.end(); ++it)
    call_list_its2.push_back(it);

  unsigned long nout2 = outlier.compute_outliers_test(func_id, call_list_its2);

  std::cout << "# outliers detected: " << nout2 << std::endl;


  //Generate some events with an outlier itr 3 different outlier val:10000
  runtimes.clear();
  std::list<ExecData_t> call_list3;  //aka CallList_t
  for(int i=0;i<N;i++){
    double val = i==N-1 ? 10000 : double(dist(gen)); //outlier on N-1
    call_list3.push_back( createFuncExecData_t(0,0,0,  func_id, "my_func", 1000*(i+1),val) );
    runtimes.push_back(val);
    //std::cout << call_list.back().get_json().dump() << std::endl;
  }

  stats_r4.create_histogram(runtimes);

  outlier.sync_param_test(&stats4);

  std::string stats_state4 = outlier.get_global_parameters()->serialize();

  std::cout << "Stats: " << stats_state4 << std::endl;

  std::vector<CallListIterator_t> call_list_its3;
  for(CallListIterator_t it=call_list3.begin(); it != call_list3.end(); ++it)
    call_list_its3.push_back(it);

  unsigned long nout3 = outlier.compute_outliers_test(func_id, call_list_its3);

  std::cout << "# outliers detected: " << nout3 << std::endl;


}


TEST(COPODADOutlierTestSyncParamWithPS, Works){
  CopodParam global_params_ps; //parameters held in the parameter server
  CopodParam local_params_ad; //parameters collected by AD

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

  CopodParam combined_params_ps; //what we expect
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
      int nt = 4;  //4 workers
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
			  ADOutlierCOPODTest outlier;
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

// TEST(CopodADOutlierTest, TestFunctionThresholdOverride){
//   int func_id =101;
//   int func_id2 = 202;
//   double default_threshold = 0.99;
//   ADOutlierCOPOD ad(default_threshold);
//   ad.overrideFuncThreshold("my_func",0.77);
//   EXPECT_EQ(ad.getFunctionThreshold("my_func"), 0.77);
//   EXPECT_EQ(ad.getFunctionThreshold("my_other_func"), default_threshold);
// }
