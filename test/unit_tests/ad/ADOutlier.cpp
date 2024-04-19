#include<chimbuko/ad/ADOutlier.hpp>
#include<chimbuko/util/Anomalies.hpp>
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

//Derived class to allow access to protected member functions
class ADOutlierSSTDTest: public ADOutlierSSTD{
public:
  ADOutlierSSTDTest(): ADOutlierSSTD(){}

  std::pair<size_t, size_t> sync_param_test(ParamInterface* param){ return this->ADOutlierSSTD::sync_param(param); }

  unsigned long compute_outliers_test(Anomalies &anomalies,
				      const unsigned long func_id, std::vector<CallListIterator_t>& data){
    ExecDataMap_t execdata;
    execdata[func_id] = data;
    
    ADExecDataInterface iface(&execdata);
    ADExecDataInterface::FunctionsSeenType fseen;
    iface.setIgnoreFirstFunctionCall(&fseen); //workaround for JIT compiled functions

    this->labelData(iface.getDataSet(0),0,iface);

    anomalies.import(iface);
    return anomalies.nEventsRecorded(Anomalies::EventType::Outlier);
  }

  double computeScoreTest(CallListIterator_t ev, const SstdParam &stats) const{
    return this->computeScore(ev->get_exclusive(),ev->get_fid(),stats);
  }

};

class ADOutlierHBOSTest: public ADOutlierHBOS{
public:
  ADOutlierHBOSTest(): ADOutlierHBOS(){}

  std::pair<size_t, size_t> sync_param_test(ParamInterface* param){ return this->ADOutlierHBOS::sync_param(param); }

  unsigned long compute_outliers_test(Anomalies &anomalies,
				      const unsigned long func_id, std::vector<CallListIterator_t>& data){
    ExecDataMap_t execdata;
    execdata[func_id] = data;
    
    ADExecDataInterface iface(&execdata);
    this->labelData(iface.getDataSet(0),0,iface);

    anomalies.import(iface);
    return anomalies.nEventsRecorded(Anomalies::EventType::Outlier);
  }

  void setParams(const HbosParam &p){ 
    ( (HbosParam*)m_param )->copy(p);
  }

  HbosParam & get_local_parameters() const{ return *((HbosParam*)m_local_param); }

  void test_updateGlobalModel(){ this->updateGlobalModel(); }
};

class ADOutlierCOPODTest: public ADOutlierCOPOD{
public:
  ADOutlierCOPODTest(): ADOutlierCOPOD(){}

  unsigned long compute_outliers_test(Anomalies &anomalies,
				      const unsigned long func_id, std::vector<CallListIterator_t>& data){
    ExecDataMap_t execdata;
    execdata[func_id] = data;
    
    ADExecDataInterface iface(&execdata);
    this->labelData(iface.getDataSet(0),0,iface);

    anomalies.import(iface);
    return anomalies.nEventsRecorded(Anomalies::EventType::Outlier);
  }

  void setParams(const CopodParam &p){ 
    ( (CopodParam*)m_param )->copy(p);
  }

};

TEST(ADExecDataInterface, Works){
  int fid1 = 1234, fid2 = 5678;
  std::list<ExecData_t> call_list;  //aka CallList_t

  std::default_random_engine gen;
  std::normal_distribution<double> dist(500.,100.), dist2(1000.,200.);
  int N = 10;
  for(int i=0;i<N;i++){
    long val(dist(gen)); 
    call_list.push_back( createFuncExecData_t(0,0,0,  fid1, "my_func", 1000*(i+1),val) );
  }
  for(int i=0;i<2*N;i++){
    long val(dist(gen)); 
    call_list.push_back( createFuncExecData_t(0,0,0,  fid2, "my_func2", 1000*(i+1),val) );
  }

  ExecDataMap_t execdata;
  std::vector<CallListIterator_t> &cits1 = execdata[fid1];
  CallListIterator_t it=call_list.begin();
  for(int i=0; i<N; i++)
    cits1.push_back(it++);
  std::vector<CallListIterator_t> &cits2 = execdata[fid2];
  for(int i=0; i<2*N; i++)
    cits2.push_back(it++);
  
  {  
    ADExecDataInterface iface(&execdata);
    
    //Check we return the right amount of data
    ASSERT_EQ(iface.nDataSets(), 2);

    //As ExecDataMap_t is unordered, the mapping of the internal dset index to the fid cannot be known in advance
    size_t dset_fids[2] = { iface.getDataSetParamIndex(0), iface.getDataSetParamIndex(1) };
    size_t dsetidx_map[2];
    if(dset_fids[0] == fid1 && dset_fids[1] == fid2){
      dsetidx_map[0] = 0; dsetidx_map[1] = 1;
    }else if(dset_fids[0] == fid2 && dset_fids[1] == fid1){
      dsetidx_map[0] = 1; dsetidx_map[1] = 0;
    }else{
      FAIL();
    }

    //Check the dset_idx map correctly to fids
    EXPECT_EQ(iface.getDataSetParamIndex(dsetidx_map[0]), fid1);
    EXPECT_EQ(iface.getDataSetParamIndex(dsetidx_map[1]), fid2);

    //Check data sets are of the correct size
    auto set1 = iface.getDataSet(dsetidx_map[0]);
    auto set2 = iface.getDataSet(dsetidx_map[1]);
    EXPECT_EQ(set1.size(), N);
    EXPECT_EQ(set2.size(), 2*N);

    //Check data are from the right function
    std::cout << "Set 1:" << std::endl;
    for(auto const & e : set1){
      auto ie = iface.getExecDataEntry(dsetidx_map[0], e.index);
      std::cout << ie->get_json().dump(1) << " ";
      EXPECT_EQ(ie->get_fid(), fid1);
    }
    std::cout << std::endl;
    std::cout << "Set 2:" << std::endl;
    for(auto const & e : set2){
      auto ie = iface.getExecDataEntry(dsetidx_map[1], e.index);
      std::cout << ie->get_json().dump(1) << " ";
      EXPECT_EQ(ie->get_fid(), fid2);
    }
  }

  {  
    //Check optional ignoring of first call
    ADExecDataInterface::FunctionsSeenType fseen;

    {
      ADExecDataInterface iface(&execdata);
      iface.setIgnoreFirstFunctionCall(&fseen);
      
      size_t dset_fids[2] = { iface.getDataSetParamIndex(0), iface.getDataSetParamIndex(1) };
      size_t dsetidx_map[2];
      if(dset_fids[0] == fid1 && dset_fids[1] == fid2){
	dsetidx_map[0] = 0; dsetidx_map[1] = 1;
      }else if(dset_fids[0] == fid2 && dset_fids[1] == fid1){
	dsetidx_map[0] = 1; dsetidx_map[1] = 0;
      }else{
	FAIL();
      }
      
      auto set1 = iface.getDataSet(dsetidx_map[0]);
      auto set2 = iface.getDataSet(dsetidx_map[1]);
      EXPECT_EQ(set1.size(), N-1);
      EXPECT_EQ(set2.size(), 2*N-1);
      
      //Check a second call to get the dataset returns the same thing (caching)
      auto set1_2 = iface.getDataSet(dsetidx_map[0]);
      auto set2_2 = iface.getDataSet(dsetidx_map[1]);
      EXPECT_EQ(set1,set1_2);
      EXPECT_EQ(set2,set2_2);
    }

    //Repeating this (with the same data but unimportant for this test) should give the same thing, but only because those first entries were labeled as normal by the above
    {
      EXPECT_EQ(cits1[0]->get_label(),1);
      EXPECT_EQ(cits2[0]->get_label(),1);

      ADExecDataInterface iface(&execdata);
      iface.setIgnoreFirstFunctionCall(&fseen);
      
      size_t dset_fids[2] = { iface.getDataSetParamIndex(0), iface.getDataSetParamIndex(1) };
      size_t dsetidx_map[2];
      if(dset_fids[0] == fid1 && dset_fids[1] == fid2){
	dsetidx_map[0] = 0; dsetidx_map[1] = 1;
      }else if(dset_fids[0] == fid2 && dset_fids[1] == fid1){
	dsetidx_map[0] = 1; dsetidx_map[1] = 0;
      }else{
	FAIL();
      }
      
      auto set1 = iface.getDataSet(dsetidx_map[0]);
      auto set2 = iface.getDataSet(dsetidx_map[1]);
      EXPECT_EQ(set1.size(), N-1);
      EXPECT_EQ(set2.size(), 2*N-1);
    }

    //Unlabeling those data again we should see the full data set restored
    {
      cits1[0]->set_label(0);
      cits2[0]->set_label(0);

      ADExecDataInterface iface(&execdata);
      iface.setIgnoreFirstFunctionCall(&fseen);
      
      size_t dset_fids[2] = { iface.getDataSetParamIndex(0), iface.getDataSetParamIndex(1) };
      size_t dsetidx_map[2];
      if(dset_fids[0] == fid1 && dset_fids[1] == fid2){
	dsetidx_map[0] = 0; dsetidx_map[1] = 1;
      }else if(dset_fids[0] == fid2 && dset_fids[1] == fid1){
	dsetidx_map[0] = 1; dsetidx_map[1] = 0;
      }else{
	FAIL();
      }
      
      auto set1 = iface.getDataSet(dsetidx_map[0]);
      auto set2 = iface.getDataSet(dsetidx_map[1]);
      EXPECT_EQ(set1.size(), N);
      EXPECT_EQ(set2.size(), 2*N);
    }
  }
}

/*

TEST(ADOutlierHBOSTestSyncParamWithoutPS, Works){
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

    EXPECT_NE(h.getMax(), std::numeric_limits<double>::lowest());
    EXPECT_NE(h.getMin(), std::numeric_limits<double>::max());
  }
  local_params_ps.assign(local_params_ps_in);

  std::cout << local_params_ps_in[0].get_json().dump();

  ADOutlierHBOSTest outlier;
  outlier.sync_param_test(&local_params_ps);

  //internal copy should be equal to global copy
  HbosParam const* g = (HbosParam const*)outlier.get_global_parameters();
  auto it = g->get_hbosstats().find(0);
  ASSERT_NE(it, g->get_hbosstats().end());
  EXPECT_NE(it->second.getHistogram().getMax(), std::numeric_limits<double>::lowest());
  EXPECT_NE(it->second.getHistogram().getMin(), std::numeric_limits<double>::max());

  EXPECT_EQ(local_params_ps.get_hbosstats(), g->get_hbosstats());
}




TEST(ADOutlierHBOSTestSyncParamFrequencyWithoutPS, Works){
  HbosParam local_params_ps;

  std::default_random_engine gen;
  std::normal_distribution<double> dist(500.,100.), dist2(1000.,200.);

  int N = 50;

  ADOutlierHBOSTest outlier;
  outlier.setGlobalModelSyncFrequency(3);

  HbosParam &local_params = outlier.get_local_parameters();
  HbosParam l, l2, g;
  {
    Histogram &h = l[0].getHistogram();
    std::vector<double> runtime(N);
    for(int i=0;i<N;i++) runtime[i] = dist(gen);
    h.create_histogram(runtime);
  }

  const HbosParam & global_params = *( (HbosParam const*)outlier.get_global_parameters() );

  //On first update we expect the global model to be set to be the same as the local model, then the local model flushed
  local_params.assign(l);
  g.assign(l);
  outlier.test_updateGlobalModel();

  EXPECT_EQ(g, global_params);
  EXPECT_EQ(local_params.size(), 0);

  //Second update should maintain the state of the local model
  local_params.assign(l);
  outlier.test_updateGlobalModel();

  EXPECT_EQ(g, global_params);
  EXPECT_EQ(local_params.size(), 1);
  EXPECT_EQ(l, local_params);

  //Third update we update the state of the local model as if we were adding new data, but the global model will again remain fixed
  local_params.update(l);
  l2.assign(l);
  l2.update(l);
  outlier.test_updateGlobalModel();
  
  EXPECT_EQ(g, global_params);
  EXPECT_EQ(local_params.size(), 1);
  EXPECT_EQ(l2, local_params);
  
  //Fourth update should update the global model and flush the local
  local_params.update(l);
  l2.update(l);
  g.update(l2);
  outlier.test_updateGlobalModel();
  
  EXPECT_EQ(g, global_params);
  EXPECT_EQ(local_params.size(), 0);
}

TEST(ADOutlierSSTDTestSyncParamWithoutPS, Works){
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

  EXPECT_EQ(local_params_ps.get_runstats().size(), 1);
  EXPECT_EQ(local_params_ps.get_runstats().begin()->second, local_params_ps_in.begin()->second);

  ADOutlierSSTDTest outlier;
  outlier.sync_param_test(&local_params_ps);

  //internal copy should be equal to global copy
  SstdParam const* glob_params = dynamic_cast<SstdParam const*>(outlier.get_global_parameters());

  EXPECT_EQ(glob_params->get_runstats(), local_params_ps.get_runstats());

  //Check serialization
  std::string glob_params_ser = glob_params->serialize();

  SstdParam glob_params_unser;
  glob_params_unser.assign(glob_params_ser);

  EXPECT_EQ(glob_params_unser.get_runstats(), local_params_ps.get_runstats());
}



TEST(ADOutlierSSTDTestSyncParamWithPS, Works){
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


TEST(ADOutlierSSTDTestComputeOutliersWithoutPS, Works){
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
  EXPECT_EQ( (unsigned long)outliers.nEventsRecorded(Anomalies::EventType::Outlier), nout);

  //Check that running again on the same data does not report new outliers
  nout = outlier.compute_outliers_test(outliers, func_id, call_list_its);
  EXPECT_EQ(nout, 0);
}


TEST(ADOutlierSSTDTestRunWithoutPS, Works){
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
  ADExecDataInterface iface(&data_map);
  outlier.run(iface,0);
  Anomalies anomalies;
  anomalies.import(iface);

  size_t nout = anomalies.nEventsRecorded(Anomalies::EventType::Outlier);
  std::cout << "# outliers detected: " << nout << std::endl;
  EXPECT_EQ(nout, 1);
  size_t nout_func = anomalies.nFuncEventsRecorded(func_id, Anomalies::EventType::Outlier);
  EXPECT_EQ(nout_func, 1);
  EXPECT_EQ( anomalies.allEventsRecorded(Anomalies::EventType::Outlier)[0], std::next(call_list.begin(), N-1) ); //last event

  //It should also have captured exactly one normal event for comparison (we don't capture all normal events as that would defeat the purpose!)
  //This should be the event with the lowest score
  size_t nnorm = anomalies.nEventsRecorded(Anomalies::EventType::Normal);
  std::cout << "# normal events recorded: " << nnorm << std::endl;
  EXPECT_EQ(nnorm, 1);
  size_t nnorm_func = anomalies.nFuncEventsRecorded(func_id, Anomalies::EventType::Normal);
  EXPECT_EQ(nnorm_func, 1);

  //Find event with lowest score
  double lscore = 1e32;
  std::list<ExecData_t>::iterator eit = call_list.end();
  int eidx;
  int i=0;
  for(auto e = call_list.begin(); e != call_list.end(); e++){
    if(e->get_outlier_score() < lscore){
      eit = e;
      eidx = i;
      lscore = e->get_outlier_score();
    }
    i++;
  }
  std::cout << "Lowest score is on event " << i << " with value " << eit->get_json().dump(1) << std::endl;
  std::cout << "Anomalies returned normal event " << anomalies.allEventsRecorded(Anomalies::EventType::Normal)[0]->get_json().dump(1) << std::endl;
 
  EXPECT_EQ( anomalies.allEventsRecorded(Anomalies::EventType::Normal)[0], eit );
}


TEST(ADOutlierSSTDTestFuncIgnore, Works){
  //Generate statistics
  std::default_random_engine gen;
  std::normal_distribution<double> dist(420.,10.);
  int N = 50;
  int func_id = 1234;
  int func_id2 = 5678;
  std::string fname1 = "my_func", fname2 = "my_other_func";

  ADOutlierSSTDTest outlier;

  std::list<ExecData_t> call_list;  //aka CallList_t
  for(int i=0;i<N;i++){
    long val = i==N-1 ? 800 : long(dist(gen)); //outlier on N-1
    call_list.push_back( createFuncExecData_t(0,0,0,  func_id, fname1, 1000*(i+1),val) );
    call_list.push_back( createFuncExecData_t(0,0,0,  func_id2, fname2, 1000*(i+1),val) );
  }
  long ts_end = 1000*N + 800;

  ExecDataMap_t data_map;
  for(CallListIterator_t it=call_list.begin(); it != call_list.end(); ++it)
    data_map[it->get_fid()].push_back(it);

  //run method generates statistics from input data map and merges with stored stats
  //thus including the outliers in the stats! Nevertheless with enough good events the stats shouldn't be poisoned too badly
  ADExecDataInterface iface(&data_map);
  iface.setIgnoreFunction(fname1);
  EXPECT_TRUE(iface.ignoringFunction(fname1));
  EXPECT_FALSE(iface.ignoringFunction(fname2));
  
  outlier.run(iface,0);
  Anomalies anomalies;
  anomalies.import(iface);

  EXPECT_EQ(anomalies.nFuncEventsRecorded(func_id, Anomalies::EventType::Outlier), 0);
  EXPECT_EQ(anomalies.nFuncEventsRecorded(func_id2, Anomalies::EventType::Outlier), 1);

  //Check all fname events are labeled normal
  for(auto const &e : call_list)
    if(e.get_fid() == func_id) EXPECT_EQ( e.get_label(), 1 );
}




TEST(ADOutlierSSTDTestRunWithoutPS, OutlierStatisticSelection){
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
    ADOutlierSSTDTest outlier;
    ADExecDataInterface iface(&data_map);
    outlier.run(iface, 0);
    Anomalies anomalies; anomalies.import(iface);

    size_t nout = anomalies.nEventsRecorded(Anomalies::EventType::Outlier);
    std::cout << "# outliers detected: " << nout << std::endl;
    EXPECT_EQ(nout, 1);
    size_t nout_par = anomalies.nFuncEventsRecorded(func_id_par, Anomalies::EventType::Outlier);
    EXPECT_EQ(nout_par, 0);
    size_t nout_child = anomalies.nFuncEventsRecorded(func_id_child, Anomalies::EventType::Outlier);
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
    ADOutlierSSTDTest outlier;
    ADExecDataInterface iface(&data_map,ADExecDataInterface::InclusiveRuntime);
    outlier.run(iface, 0);
    Anomalies anomalies; anomalies.import(iface);

    size_t nout = anomalies.nEventsRecorded(Anomalies::EventType::Outlier);
    std::cout << "# outliers detected: " << nout << std::endl;
    EXPECT_EQ(nout, 2);
    size_t nout_par = anomalies.nFuncEventsRecorded(func_id_par, Anomalies::EventType::Outlier);
    EXPECT_EQ(nout_par, 1);
    size_t nout_child = anomalies.nFuncEventsRecorded(func_id_child, Anomalies::EventType::Outlier);
    EXPECT_EQ(nout_child, 1);
  }
}


TEST(ADOutlierSSTDtest, TestAnomalyScore){
  ADOutlierSSTDTest outlier;
  
  int fid = 100;
  SstdParam params;
  RunStats & stats = params[fid];

  double mean = 100;
  stats.set_eta(mean);
  
  double std_dev = 10;
  double var = pow(std_dev,2);
  
  stats.set_count(1000);
  stats.set_rho(var * (stats.count() - 1));


  std::list<ExecData_t> events = { createFuncExecData_t(0,0,0,  100, "myfunc", 1000, mean + std_dev) };
  CallListIterator_t it = events.begin();
  
  double score = outlier.computeScoreTest(it, params);
  std::cout << "Score for " << it->get_json().dump() << " : " << score << std::endl;
  
  //double expect = (1-0.682689492137086)/2.; //half of probability of 1-sigma up/down fluctuation from https://en.wikipedia.org/wiki/68%E2%80%9395%E2%80%9399.7_rule
  double expect = 1.0; //1 std.dev discrepancy

  EXPECT_NEAR(score, expect, 1e-7);

  events.push_back(createFuncExecData_t(0,0,0,  100, "myfunc", 1000, mean - std_dev));

  it++;
  score = outlier.computeScoreTest(it, params);
  std::cout << "Score for " << it->get_json().dump() << " : " << score << std::endl;
  
  EXPECT_NEAR(score, expect, 1e-7); //should be same probability
}

TEST(ADOutlierHBOSTest, TestAnomalyDetection){
  ADOutlierHBOSTest outlier;
  
  //Generate a histogram
  std::vector<Histogram::CountType> counts = {2,8,1,0,0,2};
  HbosFuncParam fp;
  Histogram &h = fp.getHistogram();
  h.set_histogram(counts,101,700,100,100);

  //Compute the expected scores
  double alpha = 78.88e-32; //this is the default as of when the test was written! scores 0-100
  outlier.set_alpha(alpha); 

  int sum_counts = std::accumulate(counts.begin(), counts.end(), 0);
  std::vector<double> scores(counts.size());
  for(int i=0;i<counts.size();i++){
    double prob = double(counts[i])/sum_counts;
    scores[i] = -1 * log2(prob + alpha); 
  }

  int fid = 33;
  std::unordered_map<unsigned long, HbosFuncParam> stats = { {fid, fp} }; 
  
  HbosParam p;
  p.set_hbosstats(stats);
  EXPECT_EQ( p.get_hbosstats(), stats );

  outlier.setParams(p);


  //Check data point in peak bin is not an outlier
  {
    std::list<ExecData_t> events = { createFuncExecData_t(0,0,0,  fid, "myfunc", 1981, 250) };
    std::vector<CallListIterator_t> data = {events.begin()};
    Anomalies anom;
    unsigned long n = outlier.compute_outliers_test(anom, fid, data);
    EXPECT_EQ(n, 0);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Outlier), 0);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Normal), 1);
    EXPECT_EQ(anom.funcEventsRecorded(fid, Anomalies::EventType::Normal)[0], data[0]);
    EXPECT_EQ(data[0]->get_label(),1); 
    EXPECT_NEAR(data[0]->get_outlier_score(), scores[1], 1e-3);
  }
  //Check data point in the last bin is not an outlier
  {
    std::list<ExecData_t> events = { createFuncExecData_t(0,0,0,  fid, "myfunc", 1981, 650) };
    std::vector<CallListIterator_t> data = {events.begin()};
    Anomalies anom;
    unsigned long n = outlier.compute_outliers_test(anom, fid, data);
    EXPECT_EQ(n, 0);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Outlier), 0);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Normal), 1);
    EXPECT_EQ(anom.funcEventsRecorded(fid, Anomalies::EventType::Normal)[0], data[0]);
    EXPECT_EQ(data[0]->get_label(),1);
    EXPECT_NEAR(data[0]->get_outlier_score(), scores[5], 1e-3);
  }
  //Edge points within 5% of the bin width of the first and last bin should be included within that bin
  {
    std::list<ExecData_t> events = { createFuncExecData_t(0,0,0,  fid, "myfunc", 1981, 96) };
    std::vector<CallListIterator_t> data = {events.begin()};
    Anomalies anom;
    unsigned long n = outlier.compute_outliers_test(anom, fid, data);
    EXPECT_EQ(n, 0);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Outlier), 0);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Normal), 1);
    EXPECT_EQ(anom.funcEventsRecorded(fid, Anomalies::EventType::Normal)[0], data[0]);
    EXPECT_EQ(data[0]->get_label(),1);
    EXPECT_NEAR(data[0]->get_outlier_score(), scores[0], 1e-3);
  }
  {
    std::list<ExecData_t> events = { createFuncExecData_t(0,0,0,  fid, "myfunc", 1981, 704) };
    std::vector<CallListIterator_t> data = {events.begin()};
    Anomalies anom;
    unsigned long n = outlier.compute_outliers_test(anom, fid, data);
    EXPECT_EQ(n, 0);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Outlier), 0);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Normal), 1);
    EXPECT_EQ(anom.funcEventsRecorded(fid, Anomalies::EventType::Normal)[0], data[0]);
    EXPECT_EQ(data[0]->get_label(),1);
    EXPECT_NEAR(data[0]->get_outlier_score(), scores[5], 1e-3);
  }
  //Points outside of the histogram should be outliers with score = 100
  {
    std::list<ExecData_t> events = { createFuncExecData_t(0,0,0,  fid, "myfunc", 1981, 90) };
    std::vector<CallListIterator_t> data = {events.begin()};
    Anomalies anom;
    unsigned long n = outlier.compute_outliers_test(anom, fid, data);
    EXPECT_EQ(n, 1);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Outlier), 1);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Normal), 0);
    EXPECT_EQ(anom.funcEventsRecorded(fid, Anomalies::EventType::Outlier)[0], data[0]);
    EXPECT_EQ(data[0]->get_label(),-1);
    EXPECT_NEAR(data[0]->get_outlier_score(), 100., 1e-3);
  }
  {
    std::list<ExecData_t> events = { createFuncExecData_t(0,0,0,  fid, "myfunc", 1981, 710) };
    std::vector<CallListIterator_t> data = {events.begin()};
    Anomalies anom;
    unsigned long n = outlier.compute_outliers_test(anom, fid, data);
    EXPECT_EQ(n, 1);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Outlier), 1);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Normal), 0);
    EXPECT_EQ(anom.funcEventsRecorded(fid, Anomalies::EventType::Outlier)[0], data[0]);
    EXPECT_EQ(data[0]->get_label(),-1);
    EXPECT_NEAR(data[0]->get_outlier_score(), 100., 1e-3);
  }
  //Points in bins with 0 count should be outliers with score = 100
  {
    std::list<ExecData_t> events = { createFuncExecData_t(0,0,0,  fid, "myfunc", 1981, 550) };
    std::vector<CallListIterator_t> data = {events.begin()};
    Anomalies anom;
    unsigned long n = outlier.compute_outliers_test(anom, fid, data);
    EXPECT_EQ(n, 1);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Outlier), 1);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Normal), 0);
    EXPECT_EQ(anom.funcEventsRecorded(fid, Anomalies::EventType::Outlier)[0], data[0]);
    EXPECT_EQ(data[0]->get_label(),-1);
    EXPECT_NEAR(data[0]->get_outlier_score(), 100., 1e-3);
  }
}

TEST(ADOutlierHBOSTestFuncIgnore, Works){
  //Generate statistics
  std::default_random_engine gen;
  std::normal_distribution<double> dist(420.,10.);
  int N = 50;
  int func_id = 1234;
  int func_id2 = 5678;
  std::string fname1 = "my_func", fname2 = "my_other_func";

  ADOutlierHBOSTest outlier;

  std::list<ExecData_t> call_list;  //aka CallList_t
  for(int i=0;i<N;i++){
    long val = i==N-1 ? 800 : long(dist(gen)); //outlier on N-1
    call_list.push_back( createFuncExecData_t(0,0,0,  func_id, fname1, 1000*(i+1),val) );
    call_list.push_back( createFuncExecData_t(0,0,0,  func_id2, fname2, 1000*(i+1),val) );
  }
  long ts_end = 1000*N + 800;

  ExecDataMap_t data_map;
  for(CallListIterator_t it=call_list.begin(); it != call_list.end(); ++it)
    data_map[it->get_fid()].push_back(it);

  ADExecDataInterface iface(&data_map);
  iface.setIgnoreFunction(fname1);
  EXPECT_TRUE(iface.ignoringFunction(fname1));
  EXPECT_FALSE(iface.ignoringFunction(fname2));

  outlier.run(iface, 0);
  Anomalies anomalies;
  anomalies.import(iface);

  EXPECT_EQ(anomalies.nFuncEventsRecorded(func_id, Anomalies::EventType::Outlier), 0);
  EXPECT_GE(anomalies.nFuncEventsRecorded(func_id2, Anomalies::EventType::Outlier), 1); //depending on various tolerances (e.g. number of bins, number of data points), it may detect more than 1 outlier, but should detect at least the artificial one

  //Check all fname events are labeled normal
  for(auto const &e : call_list)
    if(e.get_fid() == func_id) EXPECT_EQ( e.get_label(), 1 );
}




TEST(ADOutlierCOPODTest, TestAnomalyDetection){
  ADOutlierCOPODTest outlier;
  
  //Generate a histogram
  std::vector<Histogram::CountType> counts = {2,8,1,0,0,2};
  CopodFuncParam hp;
  Histogram &h = hp.getHistogram();
  h.set_histogram(counts,101,700,100,100);

  //Compute the expected scores
  double alpha = 78.88e-32; //this is the default as of when the test was written! scores 0-100
  outlier.set_alpha(alpha); 

  int fid = 33;
  std::unordered_map<unsigned long, CopodFuncParam> stats = { {fid, hp} }; 
  
  CopodParam p;
  p.set_copodstats(stats);
  EXPECT_EQ( p.get_copodstats(), stats );

  outlier.setParams(p);

  //Histogram above is right-skewed

  //Check score for point left of histogram ; should be an outlier
  {
    std::list<ExecData_t> events = { createFuncExecData_t(0,0,0,  fid, "myfunc", 1981, 95) };
    std::vector<CallListIterator_t> data = {events.begin()};
    Anomalies anom;
    unsigned long n = outlier.compute_outliers_test(anom, fid, data);
    EXPECT_EQ(n, 1);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Outlier), 1);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Normal), 0);
    EXPECT_EQ(anom.funcEventsRecorded(fid, Anomalies::EventType::Outlier)[0], data[0]);
    EXPECT_EQ(data[0]->get_label(),-1);
  }

  //Check score for point right of histogram ; should be an outlier
  {
    std::list<ExecData_t> events = { createFuncExecData_t(0,0,0,  fid, "myfunc", 1981, 705) };
    std::vector<CallListIterator_t> data = {events.begin()};
    Anomalies anom;
    unsigned long n = outlier.compute_outliers_test(anom, fid, data);
    EXPECT_EQ(n, 1);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Outlier), 1);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Normal), 0);
    EXPECT_EQ(anom.funcEventsRecorded(fid, Anomalies::EventType::Outlier)[0], data[0]);
    EXPECT_EQ(data[0]->get_label(),-1);
  }

  //Check data point in peak bin is not an outlier
  {
    std::list<ExecData_t> events = { createFuncExecData_t(0,0,0,  fid, "myfunc", 1981, 250) };
    std::vector<CallListIterator_t> data = {events.begin()};
    Anomalies anom;
    unsigned long n = outlier.compute_outliers_test(anom, fid, data);
    EXPECT_EQ(n, 0);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Outlier), 0);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Normal), 1);
    EXPECT_EQ(anom.funcEventsRecorded(fid, Anomalies::EventType::Normal)[0], data[0]);
    EXPECT_EQ(data[0]->get_label(),1);
  }
  //Check data point lying on top of the min value is not an outlier despite the naive CDF being 0
  {
    std::list<ExecData_t> events = { createFuncExecData_t(0,0,0,  fid, "myfunc", 1981, 101) };
    std::vector<CallListIterator_t> data = {events.begin()};
    Anomalies anom;
    unsigned long n = outlier.compute_outliers_test(anom, fid, data);
    EXPECT_EQ(n, 0);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Outlier), 0);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Normal), 1);
    EXPECT_EQ(anom.funcEventsRecorded(fid, Anomalies::EventType::Normal)[0], data[0]);
    EXPECT_EQ(data[0]->get_label(),1);
  }


}



//For a multi-modal distribution it is not clear how COPOD handles data in bins between peaks
TEST(ADOutlierCOPODTest, TestAnomalyDetectionMultimodal){
  ADOutlierCOPODTest outlier;
  
  //Generate a histogram
  CopodFuncParam hp;
  Histogram &h = hp.getHistogram();
  h.set_histogram({      1,  2,  12, 1,  0,  0,  0,  0,  0,   2,   4,  16,   2}, 101,1400,100,100);

  //Compute the expected scores
  double alpha = 78.88e-32; //this is the default as of when the test was written! scores 0-100
  outlier.set_alpha(alpha); 

  int fid = 33;
  std::unordered_map<unsigned long, CopodFuncParam> stats = { {fid, hp} }; 
  
  CopodParam p;
  p.set_copodstats(stats);
  EXPECT_EQ( p.get_copodstats(), stats );

  outlier.setParams(p);

  //Histogram above should not be strongly skewed
  std::cout << "Histogram " << h << std::endl;
  std::cout << "Skewness: " << h.skewness() << std::endl;

  //Check score for point in middle of histogram ; should be an outlier
  //   Interesting, it looks like COPOD does not work with multimodal distributions
  if(0){
    std::list<ExecData_t> events = { createFuncExecData_t(0,0,0,  fid, "myfunc", 1981, 750) };
    std::vector<CallListIterator_t> data = {events.begin()};
    Anomalies anom;
    unsigned long n = outlier.compute_outliers_test(anom, fid, data);
    EXPECT_EQ(n, 1);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Outlier), 1);
    ASSERT_EQ(anom.nFuncEventsRecorded(fid, Anomalies::EventType::Normal), 0);
    EXPECT_EQ(anom.funcEventsRecorded(fid, Anomalies::EventType::Outlier)[0], data[0]);
    EXPECT_EQ(data[0]->get_label(),-1);
  }

}

TEST(ADOutlierCOPODTestFuncIgnore, Works){
  //Generate statistics
  std::default_random_engine gen;
  std::normal_distribution<double> dist(420.,10.);
  int N = 50;
  int func_id = 1234;
  int func_id2 = 5678;
  std::string fname1 = "my_func", fname2 = "my_other_func";

  ADOutlierCOPODTest outlier;

  std::list<ExecData_t> call_list;  //aka CallList_t
  for(int i=0;i<N;i++){
    long val = i==N-1 ? 800 : long(dist(gen)); //outlier on N-1
    call_list.push_back( createFuncExecData_t(0,0,0,  func_id, fname1, 1000*(i+1),val) );
    call_list.push_back( createFuncExecData_t(0,0,0,  func_id2, fname2, 1000*(i+1),val) );
  }
  long ts_end = 1000*N + 800;

  ExecDataMap_t data_map;
  for(CallListIterator_t it=call_list.begin(); it != call_list.end(); ++it)
    data_map[it->get_fid()].push_back(it);

  //run method generates statistics from input data map and merges with stored stats
  //thus including the outliers in the stats! Nevertheless with enough good events the stats shouldn't be poisoned too badly
  ADExecDataInterface iface(&data_map);
  iface.setIgnoreFunction(fname1);
  EXPECT_TRUE(iface.ignoringFunction(fname1));
  EXPECT_FALSE(iface.ignoringFunction(fname2));

  outlier.run(iface, 0);
  Anomalies anomalies;
  anomalies.import(iface);

  EXPECT_EQ(anomalies.nFuncEventsRecorded(func_id, Anomalies::EventType::Outlier), 0);
  EXPECT_EQ(anomalies.nFuncEventsRecorded(func_id2, Anomalies::EventType::Outlier), 1);

  //Check all fname events are labeled normal
  for(auto const &e : call_list)
    if(e.get_fid() == func_id) EXPECT_EQ( e.get_label(), 1 );
}



*/
