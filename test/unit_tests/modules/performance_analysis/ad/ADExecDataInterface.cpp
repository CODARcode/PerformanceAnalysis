#include<chimbuko/core/ad/ADOutlier.hpp>
#include<chimbuko/core/param/sstd_param.hpp>
#include<chimbuko/core/param/hbos_param.hpp>
#include<chimbuko/core/param/copod_param.hpp>
#include<chimbuko/core/message.hpp>
#include<chimbuko/modules/performance_analysis/ad/ADExecDataInterface.hpp>
#include "gtest/gtest.h"
#include "../../../unit_test_common.hpp"

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <random>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

std::vector<size_t> getFidIdxToDsetIdxMap(ADExecDataInterface const &iface, int fid1, int fid2)
{
  size_t dset_fids[2] = {iface.getDataSetFunctionIndex(0), iface.getDataSetFunctionIndex(1)};
  std::vector<size_t> fididx_dset_map(2); // mapping of fid index to data set index
  if (dset_fids[0] == fid1 && dset_fids[1] == fid2)
  { // dset0 -> fid1,  dset1 -> fid2
    fididx_dset_map[0] = 0;
    fididx_dset_map[1] = 1;
  }
  else if (dset_fids[0] == fid2 && dset_fids[1] == fid1)
  {
    fididx_dset_map[0] = 1;
    fididx_dset_map[1] = 0; // dset1 -> fid1, dset0 -> fid2
  }
  else
  {
    assert(0);
  } 
  return fididx_dset_map;
}

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
  
  ADglobalStringIndexMap mmap(0, MessageKind::MODEL_INDEX);
  std::unordered_map<int, std::string> fmap({ {fid1, "my_func"}, {fid2, "my_func2"} });
  std::unordered_map<int, std::string> cmap;

  {  
    ADExecDataInterface iface(&execdata, mmap, fmap, cmap);
    
    //Check we return the right amount of data
    ASSERT_EQ(iface.nDataSets(), 2);

    //As ExecDataMap_t is unordered, the mapping of the internal dset index to the fid cannot be known in advance
    auto fididx_dset_map = getFidIdxToDsetIdxMap(iface, fid1, fid2);

    //With PerFunction granularity and a local model map, the model indices should be the same as the data set indices
    EXPECT_EQ(iface.getDataSetModelIndex(0), 0);
    EXPECT_EQ(iface.getDataSetModelIndex(1), 1);

    //Check data sets are of the correct size
    auto set1 = iface.getDataSet(fididx_dset_map[0]);
    auto set2 = iface.getDataSet(fididx_dset_map[1]);
    EXPECT_EQ(set1.size(), N);
    EXPECT_EQ(set2.size(), 2*N);

    //Check data are from the right function
    std::cout << "Set 1:" << std::endl;
    for(auto const & e : set1){
      auto ie = iface.getExecDataEntry(fididx_dset_map[0], e.index);
      std::cout << ie->get_json().dump(1) << " ";
      EXPECT_EQ(ie->get_fid(), fid1);
    }
    std::cout << std::endl;
    std::cout << "Set 2:" << std::endl;
    for(auto const & e : set2){
      auto ie = iface.getExecDataEntry(fididx_dset_map[1], e.index);
      std::cout << ie->get_json().dump(1) << " ";
      EXPECT_EQ(ie->get_fid(), fid2);
    }
  }

  {  
    //Check optional ignoring of first call
    ADExecDataInterface::FunctionsSeenType fseen;

    {
      ADExecDataInterface iface(&execdata, mmap, fmap, cmap);
      iface.setIgnoreFirstFunctionCall(&fseen);
      
      auto fididx_dset_map = getFidIdxToDsetIdxMap(iface, fid1, fid2);
      
      auto set1 = iface.getDataSet(fididx_dset_map[0]);
      auto set2 = iface.getDataSet(fididx_dset_map[1]);
      EXPECT_EQ(set1.size(), N-1);
      EXPECT_EQ(set2.size(), 2*N-1);
      
      //Check a second call to get the dataset returns the same thing (caching)
      auto set1_2 = iface.getDataSet(fididx_dset_map[0]);
      auto set2_2 = iface.getDataSet(fididx_dset_map[1]);
      EXPECT_EQ(set1,set1_2);
      EXPECT_EQ(set2,set2_2);
    }

    //Repeating this (with the same data but unimportant for this test) should give the same thing, but only because those first entries were labeled as normal by the above
    {
      EXPECT_EQ(cits1[0]->get_label(),1);
      EXPECT_EQ(cits2[0]->get_label(),1);

      ADExecDataInterface iface(&execdata, mmap, fmap, cmap);
      iface.setIgnoreFirstFunctionCall(&fseen);
      
      auto fididx_dset_map = getFidIdxToDsetIdxMap(iface, fid1, fid2);
     
      auto set1 = iface.getDataSet(fididx_dset_map[0]);
      auto set2 = iface.getDataSet(fididx_dset_map[1]);
      EXPECT_EQ(set1.size(), N-1);
      EXPECT_EQ(set2.size(), 2*N-1);
    }

    //Unlabeling those data again we should see the full data set restored
    {
      cits1[0]->set_label(0);
      cits2[0]->set_label(0);

      ADExecDataInterface iface(&execdata, mmap, fmap, cmap);
      iface.setIgnoreFirstFunctionCall(&fseen);
      
      auto fididx_dset_map = getFidIdxToDsetIdxMap(iface, fid1, fid2);
      
      auto set1 = iface.getDataSet(fididx_dset_map[0]);
      auto set2 = iface.getDataSet(fididx_dset_map[1]);
      EXPECT_EQ(set1.size(), N);
      EXPECT_EQ(set2.size(), 2*N);
    }
  }
}

TEST(ADExecDataInterface, anomalyRecording){
  unsigned long fid1 = 11, fid2 = 22;
  std::list<ExecData_t> execs = { createFuncExecData_t(0,1,2,fid1,"func1",0,100),  createFuncExecData_t(3,4,5,fid2,"func2",0,100),  createFuncExecData_t(5,6,8,fid2,"func2",200,300) };

  std::vector<CallListIterator_t> exec_its;
  ExecDataMap_t execdata;
  for(auto it = execs.begin(); it != execs.end(); ++it){
    exec_its.push_back(it);
    execdata[it->get_fid()].push_back(it);
  }

  ADglobalStringIndexMap mmap(0, MessageKind::MODEL_INDEX);
  std::unordered_map<int, std::string> fmap({ {fid1, "func1"}, {fid2, "func2"} });
  std::unordered_map<int, std::string> cmap;

  { //test anomaly insert
    ADExecDataInterface iface(&execdata, mmap, fmap, cmap);
    EXPECT_EQ(iface.nEvents(), 0); //no events yet analyzed
    EXPECT_EQ(iface.nDataSets(), 2);

    int d1idx = iface.getDataSetModelIndex(0) == fid1 ? 0 : 1;
    int d2idx = !d1idx;

    std::vector<ADDataInterface::Elem> d1 = iface.getDataSet(d1idx);
    std::vector<ADDataInterface::Elem> d2 = iface.getDataSet(d2idx);
    EXPECT_EQ(d1.size(),1);
    EXPECT_EQ(d2.size(),2);

    d1[0].label = ADDataInterface::EventType::Outlier;
    iface.recordDataSetLabels(d1,d1idx);
    EXPECT_EQ(iface.nEventsRecorded(ADDataInterface::EventType::Outlier), 1);
    EXPECT_EQ(iface.nEventsRecorded(ADDataInterface::EventType::Normal), 0);
    EXPECT_EQ(iface.nEvents(), 1);

    d2[0].label = ADDataInterface::EventType::Outlier;
    d2[1].label = ADDataInterface::EventType::Outlier;
    iface.recordDataSetLabels(d2,d2idx);
    
    EXPECT_EQ(iface.nEventsRecorded(ADDataInterface::EventType::Outlier), 3);
    EXPECT_EQ(iface.nEventsRecorded(ADDataInterface::EventType::Normal), 0);
    EXPECT_EQ(iface.nEvents(), 3);

    //Check the labels have been actually assigned
    EXPECT_EQ(exec_its[0]->get_label(), -1);
    EXPECT_EQ(exec_its[1]->get_label(), -1);
    EXPECT_EQ(exec_its[2]->get_label(), -1);
  }

  { //test normal event insert
    exec_its[0]->set_label(0);
    exec_its[1]->set_label(0);
    exec_its[2]->set_label(0);

    ADExecDataInterface iface(&execdata, mmap, fmap, cmap);
    EXPECT_EQ(iface.nEvents(), 0); //no events yet analyzed
    EXPECT_EQ(iface.nDataSets(), 2);

    int d1idx = iface.getDataSetModelIndex(0) == fid1 ? 0 : 1;
    int d2idx = !d1idx;

    std::vector<ADDataInterface::Elem> d1 = iface.getDataSet(d1idx);
    std::vector<ADDataInterface::Elem> d2 = iface.getDataSet(d2idx);
    d1[0].label = d2[0].label = d2[1].label = ADDataInterface::EventType::Normal;
    d1[0].score = d2[0].score = 99;
    d2[1].score = 98; //lower score
    
    iface.recordDataSetLabels(d1,d1idx);
    iface.recordDataSetLabels(d2,d2idx);
    EXPECT_EQ(iface.nEventsRecorded(ADDataInterface::EventType::Outlier), 0);
    EXPECT_EQ(iface.nEventsRecorded(ADDataInterface::EventType::Normal), 2); //should only keep 1 normal event of each fid
    EXPECT_EQ(iface.nEvents(), 3);

    //Check the labels have been actually assigned
    EXPECT_EQ(exec_its[0]->get_label(), 1);
    EXPECT_EQ(exec_its[1]->get_label(), 1);
    EXPECT_EQ(exec_its[2]->get_label(), 1);

    auto &norm1 = iface.getResults(d1idx).getEventsRecorded(ADDataInterface::EventType::Normal);
    EXPECT_EQ(norm1.size(),1);
    EXPECT_EQ(norm1[0].index, 0);
    auto &norm2 = iface.getResults(d2idx).getEventsRecorded(ADDataInterface::EventType::Normal);
    EXPECT_EQ(norm2.size(),1);
    EXPECT_EQ(norm2[0].index, 1); //second element has lower score
  }


}



TEST(ADExecDataInterface, CounterAnomalyPerFunctionGranularity){
  int fid1 = 987, fid2 = 345;
  int cid1 = 1234, cid2 = 5678, cid3=888;
  std::list<ExecData_t> call_list;  //aka CallList_t
  ExecDataMap_t execdata;

  std::default_random_engine gen;
  std::normal_distribution<double> dist(500.,100.), dist2(1000.,200.);
  int N1 = 10, N2=13;

  ADglobalStringIndexMap mmap(0, MessageKind::MODEL_INDEX);
  
  std::unordered_map<int, std::string> fmap({ {fid1, "my_func"}, {fid2, "my_func2"} });
  std::unordered_map<int, std::string> cmap({ {cid1, "counter_1"}, {cid2, "counter_2"}, {cid3, "counter_3"} });


  //func 1, counters 1,2
  for(int i=0;i<N1;i++){
    long val(dist(gen)); 
    call_list.push_back( createFuncExecData_t(0,0,0,  fid1, "my_func", 1000*(i+1), 100) );

	  CounterData_t cnt(0,0,0, cid1, "counter_1", val, 1000*(i+1)+50);
    call_list.back().add_counter(cnt);
    execdata[fid1].push_back(std::prev(call_list.end()));
  }
  for(int i=0;i<N2;i++){
    long val(dist(gen)); 
    call_list.push_back( createFuncExecData_t(0,0,0,  fid1, "my_func", 1000*(i+1), 100) );

    CounterData_t cnt(0,0,0, cid2, "counter_2", val, 1000*(i+1)+50);
    call_list.back().add_counter(cnt);
    execdata[fid1].push_back(std::prev(call_list.end()));
  }
  //func 2, counters 1,2
  for(int i=0;i<N1;i++){
    long val(dist(gen)); 
    call_list.push_back( createFuncExecData_t(0,0,0,  fid2, "my_func2", 1000*(i+1), 100) );

	  CounterData_t cnt(0,0,0, cid1, "counter_1", val, 1000*(i+1)+50);
    call_list.back().add_counter(cnt);
    execdata[fid2].push_back(std::prev(call_list.end()));
  }
  for(int i=0;i<N2;i++){
    long val(dist(gen)); 
    call_list.push_back( createFuncExecData_t(0,0,0,  fid2, "my_func2", 1000*(i+1), 100) );

    CounterData_t cnt(0,0,0, cid2, "counter_2", val, 1000*(i+1)+50);
    call_list.back().add_counter(cnt);
    execdata[fid2].push_back(std::prev(call_list.end()));
  }

  { //check no entries returned for a counter that does not appear in the data set (but does in the map)
    ADExecDataInterface iface(&execdata, mmap, fmap, cmap, {ADExecDataInterface::Counter, cid3}, ADExecDataInterface::PerFunction);

    //Check we return the right amount of data; should have 1 data set per function
    ASSERT_EQ(iface.nDataSets(), 2);

    //PerFunction uses a different model for every function (i.e. every data set)
    EXPECT_EQ(iface.getDataSetModelIndex(0), 0);
    EXPECT_EQ(iface.getDataSetModelIndex(1), 1);

    //No function executions have this counter
    EXPECT_EQ(iface.getDataSet(0).size(), 0);
    EXPECT_EQ(iface.getDataSet(1).size(), 0);
  }
 
  {  //first counter
    ADExecDataInterface iface(&execdata, mmap, fmap, cmap, {ADExecDataInterface::Counter, cid1}, ADExecDataInterface::PerFunction);
    
    //Check we return the right amount of data; should have 1 data set per function
    ASSERT_EQ(iface.nDataSets(), 2);

    //As ExecDataMap_t is unordered, the mapping of the internal dset index to the fid cannot be known in advance
    auto fididx_dset_map = getFidIdxToDsetIdxMap(iface, fid1, fid2);
    
    //Despite being the same functions, these should be different models due to being different counters
    EXPECT_EQ(iface.getDataSetModelIndex(0), 2);
    EXPECT_EQ(iface.getDataSetModelIndex(1), 3);

    //Check data sets are of the correct size
    auto set1 = iface.getDataSet(fididx_dset_map[0]);
    auto set2 = iface.getDataSet(fididx_dset_map[1]);
    EXPECT_EQ(set1.size(), N1);
    EXPECT_EQ(set2.size(), N1);
  }

  {  //second counter
    ADExecDataInterface iface(&execdata, mmap, fmap, cmap, {ADExecDataInterface::Counter, cid2}, ADExecDataInterface::PerFunction);
    
    //Check we return the right amount of data; should have 1 data set per function
    ASSERT_EQ(iface.nDataSets(), 2);

    //As ExecDataMap_t is unordered, the mapping of the internal dset index to the fid cannot be known in advance
    auto fididx_dset_map = getFidIdxToDsetIdxMap(iface, fid1, fid2);
    
    EXPECT_EQ(iface.getDataSetModelIndex(0), 4);
    EXPECT_EQ(iface.getDataSetModelIndex(1), 5);

    //Check data sets are of the correct size
    auto set1 = iface.getDataSet(fididx_dset_map[0]);
    auto set2 = iface.getDataSet(fididx_dset_map[1]);
    EXPECT_EQ(set1.size(), N2);
    EXPECT_EQ(set2.size(), N2);
  }
}




TEST(ADExecDataInterface, CounterAnomalySingleModelGranularity){
  int fid1 = 987, fid2 = 345;
  int cid1 = 1234, cid2 = 5678, cid3=888;
  std::list<ExecData_t> call_list;  //aka CallList_t
  ExecDataMap_t execdata;

  std::default_random_engine gen;
  std::normal_distribution<double> dist(500.,100.), dist2(1000.,200.);
  int N1 = 10, N2=13;

  ADglobalStringIndexMap mmap(0, MessageKind::MODEL_INDEX);
  
  std::unordered_map<int, std::string> fmap({ {fid1, "my_func"}, {fid2, "my_func2"} });
  std::unordered_map<int, std::string> cmap({ {cid1, "counter_1"}, {cid2, "counter_2"}, {cid3, "counter_3"} });


  //func 1, counters 1,2
  for(int i=0;i<N1;i++){
    long val(dist(gen)); 
    call_list.push_back( createFuncExecData_t(0,0,0,  fid1, "my_func", 1000*(i+1), 100) );

	  CounterData_t cnt(0,0,0, cid1, "counter_1", val, 1000*(i+1)+50);
    call_list.back().add_counter(cnt);
    execdata[fid1].push_back(std::prev(call_list.end()));
  }
  for(int i=0;i<N2;i++){
    long val(dist(gen)); 
    call_list.push_back( createFuncExecData_t(0,0,0,  fid1, "my_func", 1000*(i+1), 100) );

    CounterData_t cnt(0,0,0, cid2, "counter_2", val, 1000*(i+1)+50);
    call_list.back().add_counter(cnt);
    execdata[fid1].push_back(std::prev(call_list.end()));
  }
  //func 2, counters 1,2
  for(int i=0;i<N1;i++){
    long val(dist(gen)); 
    call_list.push_back( createFuncExecData_t(0,0,0,  fid2, "my_func2", 1000*(i+1), 100) );

	  CounterData_t cnt(0,0,0, cid1, "counter_1", val, 1000*(i+1)+50);
    call_list.back().add_counter(cnt);
    execdata[fid2].push_back(std::prev(call_list.end()));
  }
  for(int i=0;i<N2;i++){
    long val(dist(gen)); 
    call_list.push_back( createFuncExecData_t(0,0,0,  fid2, "my_func2", 1000*(i+1), 100) );

    CounterData_t cnt(0,0,0, cid2, "counter_2", val, 1000*(i+1)+50);
    call_list.back().add_counter(cnt);
    execdata[fid2].push_back(std::prev(call_list.end()));
  }

  { //check no entries returned for a counter that does not appear in the data set (but does in the map)
    ADExecDataInterface iface(&execdata, mmap, fmap, cmap, {ADExecDataInterface::Counter, cid3}, ADExecDataInterface::SingleModel);

    //Check we return the right amount of data; should have 1 data set per function
    ASSERT_EQ(iface.nDataSets(), 2);

    //All data sets should have the same model index but the index should differ by counter
    EXPECT_EQ(iface.getDataSetModelIndex(0), 0);
    EXPECT_EQ(iface.getDataSetModelIndex(1), 0);

    //No function executions have this counter
    EXPECT_EQ(iface.getDataSet(0).size(), 0);
    EXPECT_EQ(iface.getDataSet(1).size(), 0);
  }
 
  {  //first counter
    ADExecDataInterface iface(&execdata, mmap, fmap, cmap, {ADExecDataInterface::Counter, cid1}, ADExecDataInterface::SingleModel);
    
    //Check we return the right amount of data; should have 1 data set per function
    ASSERT_EQ(iface.nDataSets(), 2);

    //As ExecDataMap_t is unordered, the mapping of the internal dset index to the fid cannot be known in advance
    auto fididx_dset_map = getFidIdxToDsetIdxMap(iface, fid1, fid2);
    
    //Despite being the same functions, these should be different models due to being different counters
    EXPECT_EQ(iface.getDataSetModelIndex(0), 1);
    EXPECT_EQ(iface.getDataSetModelIndex(1), 1);

    //Check data sets are of the correct size
    auto set1 = iface.getDataSet(fididx_dset_map[0]);
    auto set2 = iface.getDataSet(fididx_dset_map[1]);
    EXPECT_EQ(set1.size(), N1);
    EXPECT_EQ(set2.size(), N1);
  }

  {  //second counter
    ADExecDataInterface iface(&execdata, mmap, fmap, cmap, {ADExecDataInterface::Counter, cid2}, ADExecDataInterface::SingleModel);
    
    //Check we return the right amount of data; should have 1 data set per function
    ASSERT_EQ(iface.nDataSets(), 2);

    //As ExecDataMap_t is unordered, the mapping of the internal dset index to the fid cannot be known in advance
    auto fididx_dset_map = getFidIdxToDsetIdxMap(iface, fid1, fid2);
    
    EXPECT_EQ(iface.getDataSetModelIndex(0), 2);
    EXPECT_EQ(iface.getDataSetModelIndex(1), 2);

    //Check data sets are of the correct size
    auto set1 = iface.getDataSet(fididx_dset_map[0]);
    auto set2 = iface.getDataSet(fididx_dset_map[1]);
    EXPECT_EQ(set1.size(), N2);
    EXPECT_EQ(set2.size(), N2);
  }
}
