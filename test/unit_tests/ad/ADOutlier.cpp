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

//Derived class to allow access to protected member functions
class ADOutlierSSTDTest: public ADOutlierSSTD{
public:
  ADOutlierSSTDTest(ADOutlier::OutlierStatistic stat = ADOutlier::ExclusiveRuntime): ADOutlierSSTD(stat){

  }

  std::pair<size_t, size_t> sync_param_test(ParamInterface* param){ return this->ADOutlierSSTD::sync_param(param); }

  unsigned long compute_outliers_test(Anomalies &anomalies,
				      const unsigned long func_id, std::vector<CallListIterator_t>& data){
    return this->compute_outliers(anomalies,func_id, data);
  }
};

class ADOutlierHBOSTest: public ADOutlierHBOS{
public:
  ADOutlierSSTDTest(ADOutlier::OutlierStatistic stat = ADOutlier::ExclusiveRuntime): ADOutlierHBOS(stat){
    
  }

  std::pair<size_t, size_t> sync_param_test(ParamInterface* param){ return this->ADOutlierHBOS::sync_param(param); }

  unsigned long compute_outliers_test(Anomalies &anomalies,
				      const unsigned long func_id, std::vector<CallListIterator_t>& data){
    return this->compute_outliers(anomalies,func_id, data);
  }
};

TEST(HBOSADOutlierTestSyncParamWithoutPS, Works){
  HbosParam local_params_ps;

  std::default_random_engine gen;
  std::normal_distribution<double> dist(500.,100.);
  int N = 50;

  std::unordered_map<unsigned long, Histogram> local_params_ps_in;
  {
    Histogram &h = local_params_ps_in[0];
    std::vector<double> runtime;
    for(int i=0;i<N;i++) runtime.push_back(dist(gen));
    h.create_histogram(runtime);
  }
  local_params_ps.assign(local_params_ps_in);

  std::cout << local_params_ps_in[0].get_json().dump();

  ADOutlierHBOSTest outlier;
  outlier.sync_param_test(&local_params_ps);

  //internal copy should be equal to global copy
  std::string in_state = outlier.get_global_parameters()->serialize();

  EXPECT_EQ(local_params_ps.serialize(), in_state);
}


TEST(ADOutlierTestSyncParamWithoutPS, Works){
  SstdParam local_params_ps;

  std::default_random_engine gen;
  std::normal_distribution<double> dist(500.,100.);
  int N = 50;

  std::unordered_map<unsigned long, RunStats> local_params_ps_in;
  {
    RunStats &r = local_params_ps_in[0];
    for(int i=0;i<N;i++) r.push(dist(gen));
  }
  local_params_ps.assign(local_params_ps_in);

  std::cout << local_params_ps_in[0].get_json().dump();

  ADOutlierSSTDTest outlier;
  outlier.sync_param_test(&local_params_ps);

  //internal copy should be equal to global copy
  std::string in_state = outlier.get_global_parameters()->serialize();

  EXPECT_EQ(local_params_ps.serialize(), in_state);
}



TEST(ADOutlierTestSyncParamWithPS, Works){
  SstdParam global_params_ps; //parameters held in the parameter server
  SstdParam local_params_ad; //parameters collected by AD

  std::default_random_engine gen;
  std::normal_distribution<double> dist(500.,100.);
  int N = 50;

  {
    RunStats &r = global_params_ps[0];
    for(int i=0;i<N;i++) r.push(dist(gen));
  }

  {
    RunStats &r = local_params_ad[0];
    for(int i=0;i<N;i++) r.push(dist(gen));
  }

  std::cout << global_params_ps[0].get_json().dump();
  std::cout << local_params_ad[0].get_json().dump();

  SstdParam combined_params_ps; //what we expect
  combined_params_ps.assign(global_params_ps.get_runstats());
  combined_params_ps.update(local_params_ad.get_runstats());


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
			  ADOutlierSSTDTest outlier;
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


TEST(ADOutlierTestComputeOutliersWithoutPS, Works){
  //Generate statistics
  std::default_random_engine gen;
  std::normal_distribution<double> dist(420.,10.);
  int N = 50;
  int func_id = 1234;
  SstdParam stats;
  RunStats &stats_r = stats[func_id];
  for(int i=0;i<N;i++) stats_r.push(dist(gen));

  ADOutlierSSTDTest outlier;
  outlier.sync_param_test(&stats);

  std::string stats_state = outlier.get_global_parameters()->serialize();

  std::cout << "Stats: " << stats_state << std::endl;

  //Generate some events with an outlier

  std::list<ExecData_t> call_list;  //aka CallList_t
  for(int i=0;i<N;i++){
    long val = i==N-1 ? 800 : long(dist(gen)); //outlier on N-1
    call_list.push_back( createFuncExecData_t(0,0,0,  func_id, "my_func", 1000*(i+1),val) );
    //std::cout << call_list.back().get_json().dump() << std::endl;
  }
  long ts_end = 1000*N + 800;


  std::vector<CallListIterator_t> call_list_its;
  for(CallListIterator_t it=call_list.begin(); it != call_list.end(); ++it)
    call_list_its.push_back(it);

  Anomalies outliers;
  unsigned long nout = outlier.compute_outliers_test(outliers, func_id, call_list_its);

  std::cout << "# outliers detected: " << nout << std::endl;

  EXPECT_EQ(nout, 1);
  EXPECT_EQ( (unsigned long)outliers.nEvents(Anomalies::EventType::Outlier), nout);

  //Check that running again on the same data does not report new outliers
  nout = outlier.compute_outliers_test(outliers, func_id, call_list_its);
  EXPECT_EQ(nout, 0);
}


TEST(ADOutlierTestRunWithoutPS, Works){
  //Generate statistics
  std::default_random_engine gen;
  std::normal_distribution<double> dist(420.,10.);
  int N = 50;
  int func_id = 1234;

  ADOutlierSSTDTest outlier;

  //Generate some events with an outlier

  std::list<ExecData_t> call_list;  //aka CallList_t
  for(int i=0;i<N;i++){
    long val = i==N-1 ? 800 : long(dist(gen)); //outlier on N-1
    call_list.push_back( createFuncExecData_t(0,0,0,  func_id, "my_func", 1000*(i+1),val) );
    //std::cout << call_list.back().get_json().dump() << std::endl;
  }
  long ts_end = 1000*N + 800;

  //typedef std::unordered_map<unsigned long, std::vector<CallListIterator_t>> ExecDataMap_t;
  ExecDataMap_t data_map;
  std::vector<CallListIterator_t> &call_list_its = data_map[func_id];
  for(CallListIterator_t it=call_list.begin(); it != call_list.end(); ++it)
    call_list_its.push_back(it);

  //run method generates statistics from input data map and merges with stored stats
  //thus including the outliers in the stats! Nevertheless with enough good events the stats shouldn't be poisoned too badly
  outlier.linkExecDataMap(&data_map);
  Anomalies anomalies = outlier.run(0);

  size_t nout = anomalies.nEvents(Anomalies::EventType::Outlier);
  std::cout << "# outliers detected: " << nout << std::endl;
  EXPECT_EQ(nout, 1);
  size_t nout_func = anomalies.nFuncEvents(func_id, Anomalies::EventType::Outlier);
  EXPECT_EQ(nout_func, 1);
  EXPECT_EQ( anomalies.allEvents(Anomalies::EventType::Outlier)[0], std::next(call_list.begin(), N-1) ); //last event

  //It should also have captured exactly one normal event for comparison (we don't capture all normal events as that would defeat the purpose!)
  size_t nnorm = anomalies.nEvents(Anomalies::EventType::Normal);
  std::cout << "# normal events recorded: " << nnorm << std::endl;
  EXPECT_EQ(nnorm, 1);
  size_t nnorm_func = anomalies.nFuncEvents(func_id, Anomalies::EventType::Normal);
  EXPECT_EQ(nnorm_func, 1);
  EXPECT_EQ( anomalies.allEvents(Anomalies::EventType::Normal)[0], std::next(call_list.begin(), 0) ); //first event
}




TEST(ADOutlierTestRunWithoutPS, OutlierStatisticSelection){
  //Generate statistics
  std::default_random_engine gen;
  std::normal_distribution<double> dist(420.,10.);
  int N = 50;
  int func_id_par = 1234;
  int func_id_child = 9999;

  //Generate some events with a parent and child, for which the child has the outlier

  std::list<ExecData_t> call_list;  //aka CallList_t
  for(int i=0;i<N;i++){
    //create the parent
    unsigned long parent_start = 1000*(i+1);
    unsigned long child_start = parent_start + 1;
    long child_runtime = i==N-1 ? 800 : long(dist(gen)); //outlier on N-1
    long parent_runtime = child_runtime + 2;

    //std::cout << "Adding parent (start=" << parent_start << " runtime=" << parent_runtime << ") and child (start=" << child_start << " runtime=" << child_runtime << ")" << std::endl;

    call_list.push_back( createFuncExecData_t(0,0,0,  func_id_par, "my_parent_func", parent_start, parent_runtime) );
    call_list.push_back( createFuncExecData_t(0,0,0,  func_id_child, "my_child_func", child_start, child_runtime) );
    bindParentChild(*std::next(call_list.rbegin(),1), *call_list.rbegin() );
  }
  long ts_end = 1000*N + 800;

  //typedef std::unordered_map<unsigned long, std::vector<CallListIterator_t>> ExecDataMap_t;
  ExecDataMap_t data_map;
  for(CallListIterator_t it=call_list.begin(); it != call_list.end(); ++it)
    data_map[it->get_fid()].push_back(it);

  //Check using the exclusive runtime (default)
  {
    ADOutlierSSTDTest outlier(ADOutlier::ExclusiveRuntime);
    outlier.linkExecDataMap(&data_map);
    Anomalies anomalies = outlier.run(0);

    size_t nout = anomalies.nEvents(Anomalies::EventType::Outlier);
    std::cout << "# outliers detected: " << nout << std::endl;
    EXPECT_EQ(nout, 1);
    size_t nout_par = anomalies.nFuncEvents(func_id_par, Anomalies::EventType::Outlier);
    EXPECT_EQ(nout_par, 0);
    size_t nout_child = anomalies.nFuncEvents(func_id_child, Anomalies::EventType::Outlier);
    EXPECT_EQ(nout_child, 1);
  }

  //Unlabel all the data so we can run afresh with different settings
  for(auto fit=data_map.begin(); fit != data_map.end(); fit++){
    std::vector<CallListIterator_t> &fdata = fit->second;
    for(auto &e: fdata)
      e->set_label(0);
  }

  //Check using the include runtime; the parent should also be anomalous
  {
    ADOutlierSSTDTest outlier(ADOutlier::InclusiveRuntime);
    outlier.linkExecDataMap(&data_map);
    Anomalies anomalies = outlier.run(0);

    size_t nout = anomalies.nEvents(Anomalies::EventType::Outlier);
    std::cout << "# outliers detected: " << nout << std::endl;
    EXPECT_EQ(nout, 2);
    size_t nout_par = anomalies.nFuncEvents(func_id_par, Anomalies::EventType::Outlier);
    EXPECT_EQ(nout_par, 1);
    size_t nout_child = anomalies.nFuncEvents(func_id_child, Anomalies::EventType::Outlier);
    EXPECT_EQ(nout_child, 1);
  }
}
