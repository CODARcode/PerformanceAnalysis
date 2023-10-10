#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include<chimbuko/pserver/PSparamManager.hpp>
#include<chimbuko/param/sstd_param.hpp>
#include<chimbuko/param/hbos_param.hpp>
#include<chimbuko/util/barrier.hpp>
#include<chimbuko/net/zmq_net.hpp>
#include<chimbuko/ad/ADNetClient.hpp>
#include<chimbuko/pserver/PSfunctions.hpp>

#include<stdio.h>

using namespace chimbuko;

class PSparamManagerTest: public PSparamManager{
public:
  PSparamManagerTest(const int nworker, const std::string &ad_algorithm): PSparamManager(nworker, ad_algorithm){}
  
  using PSparamManager::getGlobalParams;
  using PSparamManager::getWorkerParams;
};


TEST(TestPSparamManager, Setup){
  //Test it fails with an invalid parameter type
  bool fail = false;
  try{
    PSparamManagerTest man(1, "test");
  }catch(const std::exception &e){
    std::cout << "Caught " << e.what() << std::endl;
    fail = true;
  }
  EXPECT_TRUE(fail);
  
  //Test it succeeds with a valid one
  fail = false;
  try{
    PSparamManagerTest man(1, "hbos");
  }catch(const std::exception &e){
    std::cout << "Caught " << e.what() << std::endl;
    fail = true;
  }
  EXPECT_FALSE(fail);
  
  //Test that getting the global parameter object fails if we try to cast to the wrong type
  fail = false;
  try{
    PSparamManagerTest man(1, "hbos");
    auto & p = man.getGlobalParams<SstdParam>();

  }catch(const std::exception &e){
    std::cout << "Caught " << e.what() << std::endl;
    fail = true;
  }
  EXPECT_TRUE(fail);

  //Test that it doesn't with the right one
  fail = false;
  try{
    PSparamManagerTest man(1, "hbos");
    auto & p = man.getGlobalParams<HbosParam>();

  }catch(const std::exception &e){
    std::cout << "Caught " << e.what() << std::endl;
    fail = true;
  }
  EXPECT_FALSE(fail);


  //Test that getting the worker parameter object fails if we try to cast to the wrong type
  fail = false;
  try{
    PSparamManagerTest man(1, "hbos");
    auto & p = man.getWorkerParams<SstdParam>(0);

  }catch(const std::exception &e){
    std::cout << "Caught " << e.what() << std::endl;
    fail = true;
  }
  EXPECT_TRUE(fail);

  //Test that it doesn't with the right one
  fail = false;
  try{
    PSparamManagerTest man(1, "hbos");
    auto & p = man.getWorkerParams<HbosParam>(0);

  }catch(const std::exception &e){
    std::cout << "Caught " << e.what() << std::endl;
    fail = true;
  }
  EXPECT_FALSE(fail);

  //Test it assigns the right number of workers
  PSparamManagerTest man1(1, "hbos");
  EXPECT_EQ(man1.nWorkers(),1);

  PSparamManagerTest man2(2, "hbos");
  EXPECT_EQ(man2.nWorkers(),2);    
}

TEST(TestPSparamManager, ManualUpdate){
  PSparamManagerTest man1(1, "hbos");
  
  HbosParam w1;
  int fid = 11;
  
  //Some fake data
  std::vector<double> d1 = {1,2,3,4,5,6,7,8,9,10,11,12};
  HbosFuncParam hp1;
  hp1.setInternalGlobalThreshold(3.141);
  hp1.getHistogram() = Histogram(d1);
  
  w1[fid] = hp1;

  //Check update fails for invalid worker index
  bool fail = false;
  try{
    man1.updateWorkerModel(w1.serialize(), 9);
  }catch(const std::exception &e){
    std::cout << "Caught " << e.what() << std::endl;
    fail = true;
  }
  EXPECT_TRUE(fail);

  //Update the worker model
  std::string glob_mod = man1.updateWorkerModel(w1.serialize(), 0);
  EXPECT_EQ(man1.getSerializedGlobalModel(), glob_mod); //check we got back the global model

  //The global model should currently be empty
  {
    HbosParam gm;
    gm.assign(glob_mod);
    EXPECT_EQ(gm.size(),0);
  }
  
  //Force the manager to update the global model
  man1.updateGlobalModel();

  {
    HbosParam gm;
    gm.assign(man1.getSerializedGlobalModel());

    //Should be equal now to w1
    auto const &stats = gm.get_hbosstats();
    EXPECT_EQ(stats.size(),1);
    auto it = stats.find(fid);
    ASSERT_NE(it, stats.end());
    
    EXPECT_EQ(it->second, hp1);
  }

  //Updating the global model again should give the same result
  man1.updateGlobalModel();

  {
    HbosParam gm;
    gm.assign(man1.getSerializedGlobalModel());

    //Should be equal now to w1
    auto const &stats = gm.get_hbosstats();
    EXPECT_EQ(stats.size(),1);
    auto it = stats.find(fid);
    ASSERT_NE(it, stats.end());
    
    EXPECT_EQ(it->second, hp1);
  }


}  


TEST(TestPSparamManager, AutoUpdate){
  PSparamManagerTest man1(1, "hbos");
  
  HbosParam w1;
  int fid = 11;
  
  //Some fake data
  std::vector<double> d1 = {1,2,3,4,5,6,7,8,9,10,11,12};
  HbosFuncParam hp1;
  hp1.setInternalGlobalThreshold(3.141);
  hp1.getHistogram() = Histogram(d1);

  w1[fid] = hp1;

  EXPECT_FALSE(man1.updaterThreadIsRunning());

  //Update the worker model first because sometimes a an updater tick can happen during the call to update leading to unpredictable (although correct) results
  std::cout << "TEST updating worker model 0" << std::endl;
  std::string glob_mod = man1.updateWorkerModel(w1.serialize(), 0);

  //Switch on the auto-updater
  man1.setGlobalModelUpdateFrequency(5000);
  std::cout << "TEST starting updater thread" << std::endl;
  man1.startUpdaterThread();

  EXPECT_TRUE(man1.updaterThreadIsRunning());

  //Wait up to 10 seconds
  int sleep_ms = 500;
  int iters = 10000/sleep_ms;
  bool good = false;
  for(int i=0;i<iters;i++){  
    std::cout << "TEST iteration " << i << " of " << iters-1 << std::endl;
    HbosParam gm;
    gm.assign(glob_mod);
    
    auto const &stats = gm.get_hbosstats();
    
    if(stats.size() !=0){
      std::cout << "Stats size " << stats.size() << std::endl;      
      EXPECT_EQ(stats.size(), 1);
      auto it = stats.find(fid);
      ASSERT_NE(it, stats.end());      
      EXPECT_EQ(it->second, hp1);
      good = true;
      break;
    }
    //std::cout << "TEST iteration " << i << " sleeping" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    glob_mod = man1.getSerializedGlobalModel();
  }  
  EXPECT_TRUE(good);
  std::cout << "TEST stopping updater thread" << std::endl;

  man1.stopUpdaterThread();
  EXPECT_FALSE(man1.updaterThreadIsRunning());
}  


TEST(TestPSparamManager, AutoUpdatePSthread){
  //Test the autoupdate using a pserver thread with multiple workers

  Barrier barrier2(2);
  std::string sinterface = "tcp://*:5559"; //default port
  std::string sname = "tcp://localhost:5559";

  std::thread psthr([&barrier2]{
      int argc; char** argv = nullptr;
      ZMQNet ps;
      ps.init(&argc, &argv, 3); //3 workers
      std::cout << "PS thread initialized net" << std::endl;

      PSparamManagerTest man(3, "hbos");
      for(int i=0;i<3;i++){
	ps.add_payload(new NetPayloadUpdateParamManager(&man, i), i);
	ps.add_payload(new NetPayloadGetParamsFromManager(&man),i);
      }
      std::cout << "PS thread initialized payload" << std::endl;     
      man.setGlobalModelUpdateFrequency(1000);
      std::cout << "PS thread starting updater thread" << std::endl;
      man.startUpdaterThread();
      std::cout << "PS thread initialized net" << std::endl;
      
      std::cout << "PS thread starting running" << std::endl;
      ps.run("."); //blocking

      //client connects
      //client runs tests
      //client disconnects

      barrier2.wait(); //1
      std::cout << "PS thread shutting down" << std::endl;
      man.stopUpdaterThread();
      ps.finalize();
    });      
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::cout << "Main thread connecting to PS" << std::endl;
  
  ADZMQNetClient net_client;
  net_client.connect_ps(0, 0, sname);

  std::cout << "Main thread connected to PS" << std::endl;

  HbosParam w1,w2,w3;
  int fid = 11;
  
  //Some fake data
  Histogram h1({1,2,3,4,5,6,7,8,9,10,11,12});
  w1[fid].getHistogram() = h1;

  Histogram h2({13,14,15,16,17,18});
  w2[fid].getHistogram() = h2;

  Histogram h3({19,20,21,22,23,24,25});
  w3[fid].getHistogram() = h3;
  
  //Data are distributed to worker threads in round-robin. Global model will then combine in order
  //NOTE: the first data is assigned to worker 1, not 0 because the handshake was given to worker 0!
  //w1 ->  worker 1
  //w2 ->  worker 2
  //w3 ->  worker 0
  //we need to combine in the same order because merge is not commutative
  HbosParam combined;
  combined.update(w3);
  combined.update(w1);
  combined.update(w2);
  
  //Send the data to the pserver
  std::cout << "Main thread sending data to PS" << std::endl;

  Message msg;
  msg.set_info(0,0, MessageType::REQ_ADD, MessageKind::PARAMETERS);
  msg.set_msg(w1.serialize(), false);
  net_client.send_and_receive(msg, msg);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

    
  msg.clear();
  msg.set_info(0,0, MessageType::REQ_ADD, MessageKind::PARAMETERS);
  msg.set_msg(w2.serialize(), false);
  net_client.send_and_receive(msg, msg);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  msg.clear();
  msg.set_info(0,0, MessageType::REQ_ADD, MessageKind::PARAMETERS);
  msg.set_msg(w3.serialize(), false);
  net_client.send_and_receive(msg, msg);


  //Wait a few seconds for the pserver to generate the global model from this data
  std::cout << "Main thread waiting for PS to process data" << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(4000));
  
  //Get the global model
  std::cout << "Main thread getting global model" << std::endl;
  msg.clear();
  msg.set_info(0,0, MessageType::REQ_GET, MessageKind::PARAMETERS);
  net_client.send_and_receive(msg, msg);

  HbosParam glob;
  glob.assign(msg.buf());

  ASSERT_TRUE(combined.find(fid));
  ASSERT_TRUE(glob.find(fid));

  Histogram const& glob_h = glob[fid].getHistogram();
  Histogram const& expect_h = combined[fid].getHistogram();
  unsigned int gc = glob_h.totalCount();
  unsigned int ec = expect_h.totalCount();
  std::cout << "Global count " << gc << " expect " << ec << std::endl;
  EXPECT_EQ(gc,ec);

  std::cout << "Global Nbin " << glob_h.Nbin() << " expect " << expect_h.Nbin() << std::endl;
  ASSERT_EQ(glob_h.Nbin(), expect_h.Nbin());

  for(int b=0;b<glob_h.Nbin();b++){
    auto gbe = glob_h.binEdges(b);
    auto ebe = expect_h.binEdges(b);
    unsigned int gbc = glob_h.binCount(b);
    unsigned int ebc = expect_h.binCount(b);

    std::cout << "Bin " << b << " global edges " << gbe.first << ":" << gbe.second << " expect " << ebe.first << ":" << ebe.second << " and global count " << gbc << " expect " << ebc << std::endl;
    EXPECT_EQ(gbe,ebe);
    EXPECT_EQ(gbc,ebc);
  } 

  EXPECT_EQ(glob_h,expect_h);

  std::cout << "Main thread disconnecting from PS" << std::endl;
  net_client.disconnect_ps();

  barrier2.wait(); //1

  psthr.join();
}

TEST(TestPSparamManager, ModelSaveRestore){
  PSparamManagerTest man1(1, "hbos");
  PSglobalFunctionIndexMap idx_map1;

  std::string fname = "my_func";

  HbosParam w1;
  unsigned fid = idx_map1.lookup(0,fname); //populate index manager


    //Some fake data
  std::vector<double> d1 = {1,2,3,4,5,6,7,8,9,10,11,12};
  HbosFuncParam hp1;
  hp1.setInternalGlobalThreshold(3.141);
  hp1.getHistogram() = Histogram(d1);
  
  w1[fid] = hp1;

  man1.updateWorkerModel(w1.serialize(), 0);
  man1.updateGlobalModel();
  
  {
    //Check current global model is as expected
    HbosParam test;
    test.assign(man1.getSerializedGlobalModel());
    
    EXPECT_TRUE(test.find(fid));
    EXPECT_EQ(test[fid], hp1);
  }

  std::string file = "model_save_restore_test.json";
  remove(file.c_str());
  std::cout << "Writing model to disk" << std::endl;
  writeModel(file, idx_map1, man1);
  
  PSparamManagerTest man2(2, "hbos");
  PSglobalFunctionIndexMap idx_map2;
  std::cout << "Restoring model from disk" << std::endl;
  restoreModel(idx_map2,man2,file);

  std::cout << "Running tests" << std::endl;

  ASSERT_TRUE(idx_map2.contains(0,fname));
  EXPECT_EQ(idx_map2.lookup(0,fname), fid);

  {
    //Check restored global model is as expected
    HbosParam test;
    test.assign(man2.getSerializedGlobalModel());
    
    EXPECT_TRUE(test.find(fid));
    EXPECT_EQ(test[fid], hp1);
  }
  
  {
    //Check worker 0 has the restored model
    HbosParam test;
    test.assign(man2.getWorkerParamsPtr(0)->serialize());
    EXPECT_TRUE(test.find(fid));
    EXPECT_EQ(test[fid], hp1);
  }
  {
    //Check worker 1 has an empty model
    HbosParam test;
    test.assign(man2.getWorkerParamsPtr(1)->serialize());
    EXPECT_EQ(test.size(), 0);
  }
  
}
