#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include<chimbuko/pserver/PSparamManager.hpp>
#include<chimbuko/param/sstd_param.hpp>
#include<chimbuko/param/hbos_param.hpp>


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
  Histogram h1(d1);
  
  w1[fid] = h1;

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
    
    EXPECT_EQ(it->second, h1);
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
    
    EXPECT_EQ(it->second, h1);
  }


}  


TEST(TestPSparamManager, AutoUpdate){
  PSparamManagerTest man1(1, "hbos");
  
  HbosParam w1;
  int fid = 11;
  
  //Some fake data
  std::vector<double> d1 = {1,2,3,4,5,6,7,8,9,10,11,12};
  Histogram h1(d1);
  
  w1[fid] = h1;

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
      EXPECT_EQ(it->second, h1);
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
  
