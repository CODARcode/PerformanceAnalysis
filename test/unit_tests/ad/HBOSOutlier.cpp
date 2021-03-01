#include<chimbuko/ad/ADOutlier.hpp>
#include<chimbuko/param/sstd_param.hpp>
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

class ADOutlierHBOSTest: public ADOutlierHBOS{
public:
  ADOutlierHBOSTest(ADOutlier::OutlierStatistic stat = ADOutlier::ExclusiveRuntime): ADOutlierHBOS(stat){}

  std::pair<size_t, size_t> sync_param_test(ParamInterface* param){ return this->ADOutlierHBOS::sync_param(param); }

  unsigned long compute_outliers_test(Anomalies &anomalies,
				      const unsigned long func_id, std::vector<CallListIterator_t>& data){
    return this->compute_outliers(anomalies,func_id, data);
  }
};

TEST(HBOSADOutlierTestSyncParamWithoutPS, Works){
  HbosParam local_params_ps;


  std::default_random_engine gen;
  std::normal_distribution<double> dist(500.,100.), dist2(1000.,200.);

  int N = 50;

  std::unordered_map<unsigned long, Histogram> local_params_ps_in;
  {
    Histogram &h = local_params_ps_in[0];
    std::vector<double> runtime;
    for(int i=0;i<N;i++) runtime.push_back(dist(gen));
    h.create_histogram(runtime);
    std::cout << "Created Histogram 1" << std::endl;

    runtime.clear();
    for(int i=0;i<100;i++) runtime.push_back(dist2(gen));
    h.merge_histograms(h, runtime);
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
  Histogram &stats_r = stats[func_id], &stats_r2 = stats2[func_id], &stats_r3 = stats3[func_id], &stats_r4 = stats4[func_id], &stats_r5 = stats5[func_id];
  std::vector<double> runtimes;
  for(int i=0;i<N;i++) runtimes.push_back(dist(gen));
  stats_r.create_histogram(runtimes);

  std::vector<double> bin_edges = stats_r.bin_edges();
  std::cout << "Bin edges 1:" << std::endl;
  for(int i=0; i<bin_edges.size(); i++) std::cout << bin_edges[i] << std::endl;

  ADOutlierHBOSTest outlier, outlier2, outlier3, outlier4;

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
  //long ts_end = 1000*N + 800;
  stats_r2.create_histogram(runtimes);
  //bin_edges.clear();
  std::vector<double> bin_edges2 = stats_r2.bin_edges();
  std::cout << "Bin edges 2:" << std::endl;
  for(int i=0; i<bin_edges2.size(); i++) std::cout << bin_edges2[i] << std::endl;
  outlier.sync_param_test(&stats2);

  std::string stats_state2 = outlier.get_global_parameters()->serialize();

  std::cout << "Stats: " << stats_state2 << std::endl;

  std::vector<CallListIterator_t> call_list_its;
  for(CallListIterator_t it=call_list.begin(); it != call_list.end(); ++it)
    call_list_its.push_back(it);

  Anomalies outliers;
  unsigned long nout = outlier.compute_outliers_test(outliers, func_id, call_list_its);

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
  //long ts_end2 = 1000*N + 800;
  stats_r3.create_histogram(runtimes);
  //bin_edges.clear();
  std::vector<double> bin_edges3 = stats_r3.bin_edges();
  std::cout << "Bin edges 3:" << std::endl;
  for(int i=0; i<bin_edges3.size(); i++) std::cout << bin_edges3[i] << std::endl;

  outlier.sync_param_test(&stats3);

  std::string stats_state3 = outlier.get_global_parameters()->serialize();

  std::cout << "Stats: " << stats_state3 << std::endl;

  std::vector<CallListIterator_t> call_list_its2;
  for(CallListIterator_t it=call_list2.begin(); it != call_list2.end(); ++it)
    call_list_its2.push_back(it);

  Anomalies outliers2;
  unsigned long nout2 = outlier.compute_outliers_test(outliers2, func_id, call_list_its2);

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
  //long ts_end2 = 1000*N + 800;
  stats_r4.create_histogram(runtimes);
  //bin_edges.clear();
  std::vector<double> bin_edges4 = stats_r4.bin_edges();
  std::cout << "Bin edges 4:" << std::endl;
  for(int i=0; i<bin_edges4.size(); i++) std::cout << bin_edges4[i] << std::endl;

  outlier.sync_param_test(&stats4);

  std::string stats_state4 = outlier.get_global_parameters()->serialize();

  std::cout << "Stats: " << stats_state4 << std::endl;

  std::vector<CallListIterator_t> call_list_its3;
  for(CallListIterator_t it=call_list3.begin(); it != call_list3.end(); ++it)
    call_list_its3.push_back(it);

  Anomalies outliers3;
  unsigned long nout3 = outlier.compute_outliers_test(outliers3, func_id, call_list_its3);

  std::cout << "# outliers detected: " << nout3 << std::endl;

  //EXPECT_EQ(nout, 1);
  //EXPECT_EQ( (unsigned long)outliers.nEvents(Anomalies::EventType::Outlier), nout);

  //Check that running again on the same data does not report new outliers
  //nout = outlier.compute_outliers_test(outliers, func_id, call_list_its);
  //EXPECT_EQ(nout, 0);
}


TEST(HBOSADOutlierTestSyncParamWithPS, Works){
  HbosParam global_params_ps; //parameters held in the parameter server
  HbosParam local_params_ad; //parameters collected by AD

  std::default_random_engine gen;
  std::normal_distribution<double> dist(500.,100.);
  int N = 50;

  {
    std::vector<double> runtimes;
    Histogram &r = global_params_ps[0];
    for(int i=0;i<N;i++) runtimes.push_back(dist(gen));
    r.create_histogram(runtimes);
  }

  {
    std::vector<double> runtimes;
    Histogram &r = local_params_ad[0];
    for(int i=0;i<N;i++) runtimes.push_back(dist(gen));
    r.create_histogram(runtimes);
  }

  std::cout << global_params_ps[0].get_json().dump();
  std::cout << local_params_ad[0].get_json().dump();

  HbosParam combined_params_ps; //what we expect
  combined_params_ps.assign(global_params_ps.get_hbosstats());
  combined_params_ps.update(local_params_ad.get_hbosstats());


#ifdef _USE_MPINET
#warning "Testing with MPINET not available"
#elif defined(_USE_ZMQNET)
  std::cout << "Using ZMQ net" << std::endl;

  Barrier barrier2(2);

  std::string sinterface = "tcp://*:5559";
  std::string sname = "tcp://localhost:5559";

  int argc; char** argv = nullptr;
  std::cout << "Initializing PS thread" << std::endl;
  std::thread ps_thr([&]{
      ZMQNet ps;
      ps.add_payload(new NetPayloadUpdateParams(&global_params_ps));
      ps.add_payload(new NetPayloadGetParams(&global_params_ps));
      ps.init(&argc, &argv, 4); //4 workers
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
			  ADNetClient net_client;
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
			//barrier2.wait();
		      });

  ps_thr.join();
  out_thr.join();

  EXPECT_EQ(glob_params_comb_ad, combined_params_ps.serialize());

#else
#error "Requires compiling with MPI or ZMQ net"
#endif
}


TEST(HBOSADOutlierTestSyncParamWithPSComputeOutliers, Works){
  HbosParam global_params_ps; //parameters held in the parameter server
  HbosParam local_params_ad, local_params_ad2; //parameters collected by AD

  std::default_random_engine gen;
  std::normal_distribution<double> dist(500.,10.);
  int N = 50;

  {
    std::vector<double> runtimes;
    Histogram &r = global_params_ps[0];
    for(int i=0;i<N;i++) runtimes.push_back(dist(gen));
    r.create_histogram(runtimes);
  }

  std::list<ExecData_t> call_list, call_list2;
  {
    // First local AD instance
    std::vector<double> runtimes;
    Histogram &r = local_params_ad[0];
    for(int i=0;i<N;i++) {
      double val = i==N-1 ? 1000 : double(dist(gen));
      call_list.push_back( createFuncExecData_t(0,0,0,  0, "my_func", 1000*(i+1), val) );
      runtimes.push_back(val);
      std::cout << "vals in localhist 1: " << val << std::endl;
    }
    r.create_histogram(runtimes);
    std::vector<double> local_bin_edges = r.bin_edges();
    std::cout << "Bin edges local:" << std::endl;
    for(int i=0; i < local_bin_edges.size(); i++) std::cout << local_bin_edges[i] << std::endl;
  }

  {
    // Second local AD instance
    std::vector<double> runtimes;
    Histogram &r = local_params_ad2[0];
    for(int i=0;i<N;i++) {
      double val = i==N-1 ? 3000 : double(dist(gen));
      call_list2.push_back( createFuncExecData_t(0,0,0,  0, "my_func", 1000*(i+1), val) );
      runtimes.push_back(val);
      std::cout << "vals in localhist 2: " << val << std::endl;
    }
    r.create_histogram(runtimes);
    std::vector<double> local_bin_edges = r.bin_edges();
    std::cout << "Bin edges local:" << std::endl;
    for(int i=0; i < local_bin_edges.size(); i++) std::cout << local_bin_edges[i] << std::endl;
  }

  std::cout << global_params_ps[0].get_json().dump();
  std::cout << local_params_ad[0].get_json().dump();
  std::cout << local_params_ad2[0].get_json().dump();

  HbosParam combined_params_ps; //what we expect
  combined_params_ps.assign(global_params_ps.get_hbosstats());
  combined_params_ps.update(local_params_ad.get_hbosstats());

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
  int argc; char** argv = nullptr;
  std::cout << "Initializing PS thread" << std::endl;
  std::thread ps_thr([&]{
      //ZMQNet ps;
      ps.add_payload(new NetPayloadUpdateParams(&global_params_ps));
      ps.add_payload(new NetPayloadGetParams(&global_params_ps));
      ps.setAutoShutdown(false);
      ps.init(&argc, &argv, 4); //4 workers
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
			  ADNetClient net_client;
			  net_client.connect_ps(0, 0, sname);
			  ADOutlierHBOSTest outlier;
			  outlier.linkNetworkClient(&net_client);
			  outlier.sync_param_test(&local_params_ad); //add local to global in PS and return to AD
			  glob_params_comb_ad  = outlier.get_global_parameters()->serialize();
        comb_params_serialize = combined_params_ps.serialize();

        Anomalies outliers;
        unsigned long nout = outlier.compute_outliers_test(outliers, 0, call_list_its);

        std::cout << "# outliers detected: " << nout << std::endl;

			  std::cout << "AD thread terminating connection" << std::endl;
			  net_client.disconnect_ps();
			  std::cout << "AD thread waiting at barrier" << std::endl;
			  barrier2.wait();
			}catch(const std::exception &e){
			  std::cerr << e.what() << std::endl;
			}
			//barrier2.wait();
		      });

  //std::this_thread::sleep_for(std::chrono::milliseconds(500));
  //EXPECT_EQ(glob_params_comb_ad, combined_params_ps.serialize());

  combined_params_ps.update(local_params_ad2.get_hbosstats());
  std::cout << "Initializing AD thread 2" << std::endl;
  std::thread out_thr2([&]{
			try{
			  ADNetClient net_client;
			  net_client.connect_ps(0, 0, sname);
			  ADOutlierHBOSTest outlier;
			  outlier.linkNetworkClient(&net_client);
			  outlier.sync_param_test(&local_params_ad2); //add local to global in PS and return to AD
			  glob_params_comb_ad2  = outlier.get_global_parameters()->serialize();
        comb_params_serialize2 = combined_params_ps.serialize();

        Anomalies outliers;
        unsigned long nout = outlier.compute_outliers_test(outliers, 0, call_list_its2);

        std::cout << "# outliers detected: " << nout << std::endl;

			  std::cout << "AD thread terminating connection" << std::endl;
			  net_client.disconnect_ps();
			  std::cout << "AD thread waiting at barrier" << std::endl;
			  barrier2.wait();
			}catch(const std::exception &e){
			  std::cerr << e.what() << std::endl;
			}
			//barrier2.wait();
		      });

  std::this_thread::sleep_for(std::chrono::milliseconds(2000);
  std::thread stop_ps([&]{ try{ps.stop();}catch(const std::exception &e) {std::cerr << e.what() << std::endl;}});
  out_thr2.join();
  out_thr.join();
  stop_ps.join();
  ps_thr.join();


  EXPECT_EQ(glob_params_comb_ad, comb_params_serialize); //combined_params_ps.serialize());
  //EXPECT_EQ(glob_params_comb_ad2, comb_params_serialize2); //combined_params_ps.serialize());
#else
#error "Requires compiling with MPI or ZMQ net"
#endif
}
