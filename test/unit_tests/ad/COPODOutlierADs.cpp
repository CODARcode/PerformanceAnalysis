#include<chimbuko/ad/ADOutlier.hpp>
#include<chimbuko/param/sstd_param.hpp>
#include<chimbuko/param/hbos_param.hpp>
#include<chimbuko/param/copod_param.hpp>
#include<chimbuko/message.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <random>

using namespace chimbuko;

class ADOutlierCOPODTest: public ADOutlierCOPOD{
public:
  ADOutlierCOPODTest(ADOutlier::OutlierStatistic stat = ADOutlier::ExclusiveRuntime): ADOutlierCOPOD(stat){}

  std::pair<size_t, size_t> sync_param_test(ParamInterface* param){ return this->ADOutlierCOPOD::sync_param(param); }

  unsigned long compute_outliers_test(Anomalies &anomalies,
				      const unsigned long func_id, std::vector<CallListIterator_t>& data){
    return this->compute_outliers(anomalies,func_id, data);
  }
};


TEST(COPODADOutlierTestSyncParamWithPSComputeOutliers, Works){
  CopodParam global_params_ps; //parameters held in the parameter server
  CopodParam local_params_ad, local_params_ad2; //parameters collected by AD

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
      double val = i==N-1 ? 10000 : double(dist(gen));
      unsigned long uval(val); //execdata truncates to unsigned long so we must do the same when generating the local histogram
      call_list.push_back( createFuncExecData_t(0,0,0,  0, "my_func", 1000*(i+1), uval) );
      runtimes.push_back(uval);
      std::cout << "vals in localhist 1: " << uval << std::endl;
    }
    r.create_histogram(runtimes);
  }

  {
    // Second local AD instance
    std::vector<double> runtimes;
    Histogram &r = local_params_ad2[0].getHistogram();
    for(int i=0;i<N;i++) {
      double val = i==N-1 ? 20000 : double(dist(gen));
      unsigned long uval(val);
      call_list2.push_back( createFuncExecData_t(0,0,0,  0, "my_func", 1000*(i+1), uval) );
      runtimes.push_back(uval);
      std::cout << "vals in localhist 2: " << uval << std::endl;
    }
    r.create_histogram(runtimes);
  }

  std::cout << global_params_ps[0].get_json().dump();
  std::cout << local_params_ad[0].get_json().dump();
  std::cout << local_params_ad2[0].get_json().dump();

  CopodParam combined_params_ps, combined_params_ps2; //what we expect
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

  Barrier barrier2(3);

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
      std::cout << "PS thread waiting at barrier" << std::endl;
      barrier2.wait();
      std::cout << "PS thread terminating connection" << std::endl;
      ps.finalize(); ps.stop();
    });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::string glob_params_comb_ad, glob_params_comb_ad2, comb_params_serialize, comb_params_serialize2;
  std::cout << "Initializing AD thread 1" << std::endl;
  std::thread out_thr([&]{
			try{
			  ADThreadNetClient net_client;
			  net_client.connect_ps(0, 0, sname);
			  ADOutlierCOPODTest outlier;
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


			  Anomalies outliers;
			  nout = outlier.compute_outliers_test(outliers, 0, call_list_its);
			  
			  std::cout << "# outliers detected: " << nout << std::endl;
			  
			  std::cout << "Global and local histograms after Outlier detection in AD 1" << std::endl;
			  std::cout << global_params_ps[0].get_json().dump();
			  std::cout << local_params_ad[0].get_json().dump();
			  std::cout << local_params_ad2[0].get_json().dump();
			  
			  std::cout << "AD thread terminating connection" << std::endl;
			  net_client.disconnect_ps();
			  std::cout << "AD thread waiting at barrier" << std::endl;
			  barrier2.wait();
			}catch(const std::exception &e){
			  std::cerr << e.what() << std::endl;
			}

		      });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));



  std::cout << "Initializing AD thread 2" << std::endl;
  std::thread out_thr2([&]{
			try{
			  ADThreadNetClient net_client;
			  net_client.connect_ps(0, 0, sname);
			  ADOutlierCOPODTest outlier;
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


        Anomalies outliers;
        nout2 = outlier.compute_outliers_test(outliers, 0, call_list_its2);

        std::cout << "# outliers detected: " << nout << std::endl;
        std::cout << "Global and local histograms after Outlier detection in AD 2" << std::endl;
        std::cout << global_params_ps[0].get_json().dump();
        std::cout << local_params_ad[0].get_json().dump();
        std::cout << local_params_ad2[0].get_json().dump();

			  std::cout << "AD thread terminating connection" << std::endl;
			  net_client.disconnect_ps();
			  std::cout << "AD thread waiting at barrier" << std::endl;
			  barrier2.wait();
			}catch(const std::exception &e){
			  std::cerr << e.what() << std::endl;
			}

		      });

  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  std::thread stop_ps([&]{ try{ps.stop();}catch(const std::exception &e) {std::cerr << e.what() << std::endl;}});
  out_thr2.join();
  out_thr.join();
  stop_ps.join();
  ps_thr.join();



  EXPECT_EQ(nout, 1);
  EXPECT_EQ(nout2, 1);
#else
#error "Requires compiling with MPI or ZMQ net"
#endif
}
