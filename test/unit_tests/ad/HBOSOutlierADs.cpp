#include<chimbuko/core/ad/ADOutlier.hpp>
#include<chimbuko/core/param/sstd_param.hpp>
#include<chimbuko/core/param/hbos_param.hpp>
#include<chimbuko/core/message.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"
#include<chimbuko/modules/performance_analysis/ad/ADEvent.hpp>
#include<chimbuko/modules/performance_analysis/ad/ADExecDataInterface.hpp>

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <random>

using namespace chimbuko;

class ADOutlierHBOSTest: public ADOutlierHBOS{
public:
  ADOutlierHBOSTest(): ADOutlierHBOS(0){}

  std::pair<size_t, size_t> sync_param_test(ParamInterface* param){ return this->ADOutlierHBOS::sync_param(param); }

  unsigned long compute_outliers_test(const unsigned long func_id, std::vector<CallListIterator_t>& data){
    ExecDataMap_t execdata;
    execdata[func_id] = data;
    
    ADExecDataInterface iface(&execdata);
    auto dset = iface.getDataSet(0);
    this->labelData(dset,0,func_id);
    iface.recordDataSetLabels(dset,0);

    return iface.nEventsRecorded(ADDataInterface::EventType::Outlier);
  }
};


TEST(HBOSADOutlierTestSyncParamWithPSComputeOutliers, Works){
  HbosParam global_params_ps; //parameters held in the parameter server
  HbosParam local_params_ad, local_params_ad2; //parameters collected by AD

  //double threshold = 0.99; //suitable for score range method
  double threshold = 0.98; //suitable for bin prob ratio method

  std::default_random_engine gen;
  std::normal_distribution<double> dist(50.,10.);
  int N = 50;

  {
    std::vector<double> runtimes;
    Histogram &r = global_params_ps[0].getHistogram();
    for(int i=0;i<N;i++) runtimes.push_back(dist(gen));
    r.create_histogram(runtimes);
  }

  std::list<ExecData_t> call_list, call_list2;
  {
    // First local AD instance
    std::vector<double> runtimes;
    Histogram &r = local_params_ad[0].getHistogram();
    for(int i=0;i<N;i++) {
      double val = i==N-1 ? 1000 : double(dist(gen));
      call_list.push_back( createFuncExecData_t(0,0,0,  0, "my_func", 1000*(i+1), val) );
      runtimes.push_back(val);
      std::cout << "vals in localhist 1: " << val << std::endl;
    }
    r.create_histogram(runtimes);
  }

  {
    // Second local AD instance
    std::vector<double> runtimes;
    Histogram &r = local_params_ad2[0].getHistogram();
    for(int i=0;i<N;i++) {
      double val = i==N-1 ? 2000 : double(dist(gen));
      call_list2.push_back( createFuncExecData_t(0,0,0,  0, "my_func", 1000*(i+1), val) );
      runtimes.push_back(val);
      std::cout << "vals in localhist 2: " << val << std::endl;
    }
    r.create_histogram(runtimes);
  }

  std::cout << global_params_ps[0].get_json().dump();
  std::cout << local_params_ad[0].get_json().dump();
  std::cout << local_params_ad2[0].get_json().dump();

  HbosParam combined_params_ps, combined_params_ps2; //what we expect
  combined_params_ps.assign(global_params_ps);
  combined_params_ps.update(local_params_ad);

  combined_params_ps2.assign(global_params_ps);
  combined_params_ps2.update(local_params_ad);
  combined_params_ps2.update(local_params_ad2);



  std::vector<CallListIterator_t> call_list_its, call_list_its2;
  for(CallListIterator_t it=call_list.begin(); it != call_list.end(); ++it)
    call_list_its.push_back(it);

  for(CallListIterator_t it=call_list2.begin(); it != call_list2.end(); ++it)
    call_list_its2.push_back(it);

#ifdef _USE_MPINET
#warning "Testing with MPINET not available"
#elif defined(_USE_ZMQNET)
  std::cout << "Using ZMQ net" << std::endl;

  Barrier barrier2(2);

  std::string sinterface = "tcp://*:5559";
  std::string sname = "tcp://localhost:5559";
  ZMQNet ps;
  unsigned long nout, nout2; //number of outliers

  int argc; char** argv = nullptr;
  std::cout << "Initializing PS thread" << std::endl;
  std::thread ps_thr([&]{
      int nt = 4; //4 workers
      for(int i=0;i<nt;i++){
	ps.add_payload(new NetPayloadUpdateParams(&global_params_ps),i);
	ps.add_payload(new NetPayloadGetParams(&global_params_ps),i);
      }
      ps.setAutoShutdown(false);
      ps.init(&argc, &argv, nt);
      ps.run(".");
      ps.finalize();
    });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::string glob_params_comb_ad, glob_params_comb_ad2, comb_params_serialize, comb_params_serialize2;
  std::cout << "Initializing AD thread 1" << std::endl;
  std::thread out_thr([&]{
      try{
	ADThreadNetClient net_client;
	net_client.connect_ps(0, 0, sname);
	ADOutlierHBOSTest outlier;
	outlier.linkNetworkClient(&net_client);

        std::cout << "Global and local histograms before sync_param in AD 1" << std::endl;
        std::cout << global_params_ps[0].get_json().dump();
        std::cout << local_params_ad[0].get_json().dump();
        std::cout << local_params_ad2[0].get_json().dump();

	outlier.sync_param_test(&local_params_ad); //add local to global in PS and return to AD

        std::cout << "Global and local histograms after sync_param in AD 1" << std::endl;
        std::cout << global_params_ps[0].get_json().dump();
        std::cout << local_params_ad[0].get_json().dump();
        std::cout << local_params_ad2[0].get_json().dump();

	glob_params_comb_ad  = outlier.get_global_parameters()->serialize();

        nout = outlier.compute_outliers_test(0, call_list_its);

        std::cout << "# outliers detected: " << nout << std::endl;

        std::cout << "Global and local histograms after Outlier detection in AD 1" << std::endl;
        std::cout << global_params_ps[0].get_json().dump();
        std::cout << local_params_ad[0].get_json().dump();
        std::cout << local_params_ad2[0].get_json().dump();

	std::cout << "AD thread terminating connection" << std::endl;
	net_client.disconnect_ps();
	barrier2.wait();
      }catch(const std::exception &e){
	std::cerr << e.what() << std::endl;
      }

    });

  std::cout << "Initializing AD thread 2" << std::endl;
  std::thread out_thr2([&]{
      try{
	barrier2.wait(); //enforce thread ordering
	ADThreadNetClient net_client;
	net_client.connect_ps(0, 0, sname);
	ADOutlierHBOSTest outlier;

	outlier.set_threshold(threshold);
	outlier.linkNetworkClient(&net_client);

	std::cout << "Global and local histograms before sync_param in AD 2" << std::endl;
	std::cout << global_params_ps[0].get_json().dump();
	std::cout << local_params_ad[0].get_json().dump();
	std::cout << local_params_ad2[0].get_json().dump();

	outlier.sync_param_test(&local_params_ad2); //add local to global in PS and return to AD

	std::cout << "Global and local histograms after sync_param in AD 2" << std::endl;
	std::cout << global_params_ps[0].get_json().dump();
	std::cout << local_params_ad[0].get_json().dump();
	std::cout << local_params_ad2[0].get_json().dump();

	glob_params_comb_ad2  = outlier.get_global_parameters()->serialize();

	nout2 = outlier.compute_outliers_test(0, call_list_its2);

	std::cout << "# outliers detected: " << nout2 << std::endl;
	std::cout << "Global and local histograms after Outlier detection in AD 2" << std::endl;
	std::cout << global_params_ps[0].get_json().dump();
	std::cout << local_params_ad[0].get_json().dump();
	std::cout << local_params_ad2[0].get_json().dump();

	std::cout << "AD thread terminating connection" << std::endl;
	net_client.disconnect_ps();
      }catch(const std::exception &e){
	std::cerr << e.what() << std::endl;
      }

    });

  out_thr2.join();
  out_thr.join();

  std::thread stop_ps([&]{ try{ps.stop();}catch(const std::exception &e) {std::cerr << e.what() << std::endl;}});
  stop_ps.join();
  ps_thr.join();

  EXPECT_GE(nout, 1);
  EXPECT_GE(nout2, 1);
#else
#error "Requires compiling with MPI or ZMQ net"
#endif
}
