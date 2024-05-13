#include<chimbuko/core/ad/ADOutlier.hpp>
#include<chimbuko/core/param/sstd_param.hpp>
#include<chimbuko/core/param/hbos_param.hpp>
#include<chimbuko/core/param/copod_param.hpp>
#include<chimbuko/core/message.hpp>
#include<chimbuko/modules/performance_analysis/ad/ADExecDataInterface.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <random>

using namespace chimbuko;

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
    size_t dset_fids[2] = { iface.getDataSetModelIndex(0), iface.getDataSetModelIndex(1) };
    size_t dsetidx_map[2];
    if(dset_fids[0] == fid1 && dset_fids[1] == fid2){
      dsetidx_map[0] = 0; dsetidx_map[1] = 1;
    }else if(dset_fids[0] == fid2 && dset_fids[1] == fid1){
      dsetidx_map[0] = 1; dsetidx_map[1] = 0;
    }else{
      FAIL();
    }

    //Check the dset_idx map correctly to fids
    EXPECT_EQ(iface.getDataSetModelIndex(dsetidx_map[0]), fid1);
    EXPECT_EQ(iface.getDataSetModelIndex(dsetidx_map[1]), fid2);

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
      
      size_t dset_fids[2] = { iface.getDataSetModelIndex(0), iface.getDataSetModelIndex(1) };
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
      
      size_t dset_fids[2] = { iface.getDataSetModelIndex(0), iface.getDataSetModelIndex(1) };
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
      
      size_t dset_fids[2] = { iface.getDataSetModelIndex(0), iface.getDataSetModelIndex(1) };
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



TEST(ADExecDataInterface, anomalyRecording){
  unsigned long fid1 = 11, fid2 = 22;
  std::list<ExecData_t> execs = { createFuncExecData_t(0,1,2,fid1,"func1",0,100),  createFuncExecData_t(3,4,5,fid2,"func2",0,100),  createFuncExecData_t(5,6,8,fid2,"func2",200,300) };

  std::vector<CallListIterator_t> exec_its;
  ExecDataMap_t execdata;
  for(auto it = execs.begin(); it != execs.end(); ++it){
    exec_its.push_back(it);
    execdata[it->get_fid()].push_back(it);
  }


  { //test anomaly insert
    ADExecDataInterface iface(&execdata);
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

    //We should get an error if we don't assign all the labels
    d2[0].label = ADDataInterface::EventType::Outlier;
    bool fail = false;
    try{
      iface.recordDataSetLabels(d2,d2idx);
    }catch(const std::exception &e){
      std::cout << "Caught expected error: " << e.what() << std::endl;
      fail = true;
    }
    ASSERT_TRUE(fail);
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

    ADExecDataInterface iface(&execdata);
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
