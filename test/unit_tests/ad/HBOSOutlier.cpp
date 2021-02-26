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
  HbosParam stats, stats2;
  Histogram &stats_r = stats[func_id], &stats_r2 = stats2[func_id];
  std::vector<double> runtimes;
  for(int i=0;i<N;i++) runtimes.push_back(dist(gen));
  stats_r.create_histogram(runtimes);

  std::vector<double> bin_edges = stats_r.bin_edges();
  std::cout << "Bin edges 1:" << std::endl;
  for(int i=0; i<bin_edges.size(); i++) std::cout << bin_edges[i] << std::endl;

  ADOutlierHBOSTest outlier;
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
  long ts_end = 1000*N + 800;
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

  //EXPECT_EQ(nout, 1);
  //EXPECT_EQ( (unsigned long)outliers.nEvents(Anomalies::EventType::Outlier), nout);

  //Check that running again on the same data does not report new outliers
  //nout = outlier.compute_outliers_test(outliers, func_id, call_list_its);
  //EXPECT_EQ(nout, 0);
}
