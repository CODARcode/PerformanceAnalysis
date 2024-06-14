#include <chimbuko_config.h>
#include <gtest/gtest.h>
#include <cassert>
#include <chimbuko/core/ad/ADProvenanceDBclient.hpp>
#include <chimbuko/core/verbose.hpp>
#include <chimbuko/core/util/string.hpp>
#include <chimbuko/modules/performance_analysis/provdb/ProvDBmoduleSetup.hpp>
#ifdef USE_MPI
#include <mpi.h>
#endif
#include <dirent.h>
#include <regex>
#include <thread>
#include <cstdio>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

//For these tests the provenance DB admin must be running
std::string addr;
int rank;
int nshards;
std::string rank_str;
ProvDBmoduleSetup setup;

TEST(ADProvenanceDBclientTest, Connects){

  bool connect_fail = false;  
  ADProvenanceDBclient client(setup.getMainDBcollections(),rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);
  }catch(const std::exception &ex){
    connect_fail = true;
  }
  EXPECT_EQ(connect_fail, false);
}

TEST(ADProvenanceDBclientTest, ConnectsTwice){

  bool connect_fail = false;
  ADProvenanceDBclient client(setup.getMainDBcollections(),rank);
  std::cout << "Client attempting connection for second time" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);
  }catch(const std::exception &ex){
    connect_fail = true;
  }
  EXPECT_EQ(connect_fail, false);
}

TEST(ADProvenanceDBclientTest, SendReceiveAnomalyData){

  bool fail = false;
  ADProvenanceDBclient client(setup.getMainDBcollections(),rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    nlohmann::json obj;
    obj["hello"] = "world " + rank_str;
    std::cout << "Sending " << obj.dump() << std::endl;
    uint64_t rid = client.sendData(obj, "anomalies");
    EXPECT_NE(rid, -1);
    
    nlohmann::json check;
    EXPECT_EQ( client.retrieveData(check, rid, "anomalies"), true );
    
    std::cout << "Testing retrieved anomaly data:" << check.dump() << std::endl;

    //NB, retrieval adds extra __id field, so objects not identical
    bool same = check["hello"] == "world " + rank_str;
    std::cout << "JSON objects are the same? " << same << std::endl;

    EXPECT_EQ(same, true);
  }catch(const std::exception &ex){
    fail = true;
  }
  EXPECT_EQ(fail, false);
}


TEST(ADProvenanceDBclientTest, SendReceiveMetadata){

  bool fail = false;
  ADProvenanceDBclient client(setup.getMainDBcollections(),rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    nlohmann::json obj;
    obj["hello"] = "world " + rank_str;
    std::cout << "Sending " << obj.dump() << std::endl;
    uint64_t rid = client.sendData(obj, "metadata");
    EXPECT_NE(rid, -1);
    
    nlohmann::json check;
    EXPECT_EQ( client.retrieveData(check, rid, "metadata"), true );
    
    std::cout << "Testing retrieved metadata:" << check.dump() << std::endl;

    //NB, retrieval adds extra __id field, so objects not identical
    bool same = check["hello"] == "world " + rank_str;
    std::cout << "JSON objects are the same? " << same << std::endl;

    EXPECT_EQ(same, true);
  }catch(const std::exception &ex){
    fail = true;
  }
  EXPECT_EQ(fail, false);
}


TEST(ADProvenanceDBclientTest, SendReceiveNormalExecData){

  bool fail = false;
  ADProvenanceDBclient client(setup.getMainDBcollections(),rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    nlohmann::json obj;
    obj["hello"] = "world " + rank_str;
    std::cout << "Sending " << obj.dump() << std::endl;
    uint64_t rid = client.sendData(obj, "normalexecs");
    EXPECT_NE(rid, -1);
    
    nlohmann::json check;
    EXPECT_EQ( client.retrieveData(check, rid, "normalexecs"), true );
    
    std::cout << "Testing retrieved normal exec:" << check.dump() << std::endl;

    //NB, retrieval adds extra __id field, so objects not identical
    bool same = check["hello"] == "world " + rank_str;
    std::cout << "JSON objects are the same? " << same << std::endl;

    EXPECT_EQ(same, true);
  }catch(const std::exception &ex){
    fail = true;
  }
  EXPECT_EQ(fail, false);
}




TEST(ADProvenanceDBclientTest, SendReceiveVectorAnomalyData){

  bool fail = false;
  ADProvenanceDBclient client(setup.getMainDBcollections(),rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    std::vector<nlohmann::json> objs(2);
    objs[0]["hello"] = "world " + rank_str;
    objs[1]["hello"] = "again " + rank_str;

    std::cout << "Sending " << std::endl << objs[0].dump() << std::endl << objs[1].dump() << std::endl;
    std::vector<uint64_t> rid = client.sendMultipleData(objs, "anomalies");
    EXPECT_EQ(rid.size(), 2);

    for(int i=0;i<2;i++){    
      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, rid[i], "anomalies"), true );
    
      std::cout << "Testing retrieved anomaly data:" << check.dump() << std::endl;

      //NB, retrieval adds extra __id field, so objects not identical
      bool same = check["hello"] ==  (i==0? "world " + rank_str : "again " + rank_str);
      std::cout << "JSON objects are the same? " << same << std::endl;
      EXPECT_EQ(same, true);
    }
  }catch(const std::exception &ex){
    fail = true;
  }
  EXPECT_EQ(fail, false);
}

TEST(ADProvenanceDBclientTest, SendReceiveJSONarrayAnomalyData){

  bool fail = false;
  ADProvenanceDBclient client(setup.getMainDBcollections(),rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    nlohmann::json objs = nlohmann::json::array();
    objs[0] = nlohmann::json::object({ {"hello","myfriend " + rank_str} });
    objs[1] = nlohmann::json::object({ {"hello","what? " + rank_str} });

    std::cout << "Sending " << std::endl << objs.dump() << std::endl;
    std::vector<uint64_t> rid = client.sendMultipleData(objs, "anomalies");
    EXPECT_EQ(rid.size(), 2);

    for(int i=0;i<2;i++){    
      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, rid[i], "anomalies"), true );
    
      std::cout << "Testing retrieved anomaly data:" << check.dump() << " with index " << rid[i] << std::endl;

      //NB, retrieval adds extra __id field, so objects not identical
      bool same = check["hello"] ==  (i==0? "myfriend " + rank_str : "what? " + rank_str);
      std::cout << "JSON objects are the same? " << same << std::endl;
      EXPECT_EQ(same, true);
    }
  }catch(const std::exception &ex){
    fail = true;
  }
  EXPECT_EQ(fail, false);
}

TEST(ADProvenanceDBclientTest, SendReceiveAnomalyDataAsync){

  bool fail = false;
  ADProvenanceDBclient client(setup.getMainDBcollections(),rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    nlohmann::json obj;
    obj["hello"] = "world " + rank_str;
    std::cout << "Sending " << obj.dump() << " asynchronously" << std::endl;

    OutstandingRequest req;

    client.sendDataAsync(obj, "anomalies", &req);

    req.wait(); //wait for completion
    int rid = req.ids[0];
    
    nlohmann::json check;
    EXPECT_EQ( client.retrieveData(check, rid, "anomalies"), true );
    
    std::cout << "Testing retrieved anomaly data:" << check.dump() << std::endl;

    //NB, retrieval adds extra __id field, so objects not identical
    bool same = check["hello"] == "world " + rank_str;
    std::cout << "JSON objects are the same? " << same << std::endl;

    EXPECT_EQ(same, true);
  }catch(const std::exception &ex){
    fail = true;
  }
  EXPECT_EQ(fail, false);
}


TEST(ADProvenanceDBclientTest, SendReceiveVectorAnomalyDataAsync){

  bool fail = false;
  ADProvenanceDBclient client(setup.getMainDBcollections(),rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    std::vector<nlohmann::json> objs(2);
    objs[0]["hello"] = "world " + rank_str;
    objs[1]["hello"] = "again " + rank_str;

    OutstandingRequest req;
    
    std::cout << "Sending " << std::endl << objs[0].dump() << std::endl << objs[1].dump() << std::endl;
    client.sendMultipleDataAsync(objs, "anomalies",&req);
    EXPECT_EQ(req.ids.size(), 2);

    req.wait(); //wait for completion
    
    for(int i=0;i<2;i++){    
      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, req.ids[i], "anomalies"), true );
    
      std::cout << "Testing retrieved anomaly data:" << check.dump() << std::endl;

      //NB, retrieval adds extra __id field, so objects not identical
      bool same = check["hello"] ==  (i==0? "world " + rank_str : "again " + rank_str);
      std::cout << "JSON objects are the same? " << same << std::endl;
      EXPECT_EQ(same, true);
    }
  }catch(const std::exception &ex){
    fail = true;
  }
  EXPECT_EQ(fail, false);
}


TEST(ADProvenanceDBclientTest, SendReceiveJSONarrayAnomalyDataAsync){

  bool fail = false;
  ADProvenanceDBclient client(setup.getMainDBcollections(),rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    nlohmann::json objs = nlohmann::json::array();
    objs[0] = nlohmann::json::object({ {"hello","myfriend " + rank_str} });
    objs[1] = nlohmann::json::object({ {"hello","what? " + rank_str} });

    OutstandingRequest req;

    std::cout << "Sending " << std::endl << objs.dump() << std::endl;
    client.sendMultipleDataAsync(objs, "anomalies", &req);
    EXPECT_EQ(req.ids.size(), 2);

    req.wait(); //wait for completion

    for(int i=0;i<2;i++){    
      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, req.ids[i], "anomalies"), true );
    
      std::cout << "Testing retrieved anomaly data:" << check.dump() << " with index " << req.ids[i] << std::endl;

      //NB, retrieval adds extra __id field, so objects not identical
      bool same = check["hello"] ==  (i==0? "myfriend " + rank_str : "what? " + rank_str);
      std::cout << "JSON objects are the same? " << same << std::endl;
      EXPECT_EQ(same, true);
    }
  }catch(const std::exception &ex){
    fail = true;
  }
  EXPECT_EQ(fail, false);
}


bool fileExistsMatchingRegex(const std::string &dir, const std::regex &regex){
  DIR* dirp = opendir(dir.c_str());
  if (dirp == NULL) return false;

  dirent* dp;
  while ((dp = readdir(dirp)) != NULL) {
    std::string fn(dp->d_name);
    bool m = std::regex_search(fn, regex);
    if(m){
      closedir(dirp);
      return true;
    }
  }
  closedir(dirp);
  return false;
}

void deleteFilesMatchingRegex(const std::string &dir, const std::regex &regex){
  DIR* dirp = opendir(dir.c_str());
  if (dirp == NULL) return;

  dirent* dp;
  while ((dp = readdir(dirp)) != NULL) {
    std::string fn(dp->d_name);
    bool m = std::regex_search(fn, regex);
    if(m){
      remove(fn.c_str());
    }
  }
  closedir(dirp);
}



#ifdef ENABLE_MARGO_STATE_DUMP
TEST(ADProvenanceDBclientTest, TestStateDump){
  if(rank == 0){
    std::regex fn(R"(margo_dump\.)");
    deleteFilesMatchingRegex(".", fn);

    if(fileExistsMatchingRegex(".", fn))
      throw std::runtime_error("Existing file of form margo_dump.* found in test directory, these should be deleted!");

    bool fail = true;
    ADProvenanceDBclient client(setup.getMainDBcollections(),rank);
    std::cout << "Client attempting connection" << std::endl;
    try{
      client.connectSingleServer(addr,nshards);

      client.serverDumpState();

      int timeout = 20;
      for(int i=0;i<timeout;i++){
	if(fileExistsMatchingRegex(".", fn)){
	  fail = false;
	  break;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      }
    }catch(const std::exception &ex){
      fail = true;
    }
    EXPECT_EQ(fail, false);
  }
}
#endif


int main(int argc, char** argv) 
{
#ifdef USE_MPI
  MPI_Init(&argc, &argv);
#endif
  chimbuko::enableVerboseLogging() = true;
  assert(argc >= 3);
  addr = argv[1];
  std::cout << "Provenance DB admin is on address: " << addr << std::endl;
  nshards = strToAny<int>(argv[2]);
  std::cout << "Number of shards is " << nshards << std::endl;

#ifdef USE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#else
  rank = 0;
#endif

  rank_str = anyToStr(rank);

  ::testing::InitGoogleTest(&argc, argv);
  int ret = RUN_ALL_TESTS();

#ifdef USE_MPI
  MPI_Finalize();
#endif
  return ret;
}
