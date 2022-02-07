#include <chimbuko_config.h>
#include <gtest/gtest.h>
#include <cassert>
#include <chimbuko/ad/ADProvenanceDBclient.hpp>
#include <chimbuko/verbose.hpp>
#include <chimbuko/util/string.hpp>
#ifdef USE_MPI
#include <mpi.h>
#endif
#include <dirent.h>
#include <regex>
#include <thread>

using namespace chimbuko;

//For these tests the provenance DB admin must be running
std::string addr;
int rank;
int ranks;
int nshards;
std::string rank_str;

TEST(ADProvenanceDBclientTest, Connects){

  bool connect_fail = false;
  ADProvenanceDBclient client(rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);
  }catch(const std::exception &ex){
    std::cout << "ERROR caught: " << ex.what() << std::endl;
    connect_fail = true;
  }
  EXPECT_EQ(connect_fail, false);
}

TEST(ADProvenanceDBclientTest, ConnectsTwice){

  bool connect_fail = false;
  ADProvenanceDBclient client(rank);
  std::cout << "Client attempting connection for second time" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);
  }catch(const std::exception &ex){
    std::cout << "ERROR caught: " << ex.what() << std::endl;
    connect_fail = true;
  }
  EXPECT_EQ(connect_fail, false);
}

TEST(ADProvenanceDBclientTest, SendReceiveAnomalyData){

  bool fail = false;
  ADProvenanceDBclient client(rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    nlohmann::json obj;
    obj["hello"] = "world " + rank_str;
    std::cout << "Sending " << obj.dump() << std::endl;
    yk_id_t rid = client.sendData(obj, ProvenanceDataType::AnomalyData);
    EXPECT_NE(rid, -1);
    
    nlohmann::json check;
    EXPECT_EQ( client.retrieveData(check, rid, ProvenanceDataType::AnomalyData), true );
    
    std::cout << "Testing retrieved anomaly data:" << check.dump() << std::endl;

    //NB, retrieval adds extra __id field, so objects not identical
    bool same = check["hello"] == "world " + rank_str;
    std::cout << "JSON objects are the same? " << same << std::endl;

    EXPECT_EQ(same, true);
  }catch(const std::exception &ex){
    std::cout << "ERROR caught: " << ex.what() << std::endl;
    fail = true;
  }
  EXPECT_EQ(fail, false);
}


TEST(ADProvenanceDBclientTest, SendReceiveMetadata){

  bool fail = false;
  ADProvenanceDBclient client(rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    nlohmann::json obj;
    obj["hello"] = "world " + rank_str;
    std::cout << "Sending " << obj.dump() << std::endl;
    yk_id_t rid = client.sendData(obj, ProvenanceDataType::Metadata);
    EXPECT_NE(rid, -1);
    
    nlohmann::json check;
    EXPECT_EQ( client.retrieveData(check, rid, ProvenanceDataType::Metadata), true );
    
    std::cout << "Testing retrieved metadata:" << check.dump() << std::endl;

    //NB, retrieval adds extra __id field, so objects not identical
    bool same = check["hello"] == "world " + rank_str;
    std::cout << "JSON objects are the same? " << same << std::endl;

    EXPECT_EQ(same, true);
  }catch(const std::exception &ex){
    std::cout << "ERROR caught: " << ex.what() << std::endl;
    fail = true;
  }
  EXPECT_EQ(fail, false);
}


TEST(ADProvenanceDBclientTest, SendReceiveNormalExecData){

  bool fail = false;
  ADProvenanceDBclient client(rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    nlohmann::json obj;
    obj["hello"] = "world " + rank_str;
    std::cout << "Sending " << obj.dump() << std::endl;
    yk_id_t rid = client.sendData(obj, ProvenanceDataType::NormalExecData);
    EXPECT_NE(rid, -1);
    
    nlohmann::json check;
    EXPECT_EQ( client.retrieveData(check, rid, ProvenanceDataType::NormalExecData), true );
    
    std::cout << "Testing retrieved normal exec:" << check.dump() << std::endl;

    //NB, retrieval adds extra __id field, so objects not identical
    bool same = check["hello"] == "world " + rank_str;
    std::cout << "JSON objects are the same? " << same << std::endl;

    EXPECT_EQ(same, true);
  }catch(const std::exception &ex){
    std::cout << "ERROR caught: " << ex.what() << std::endl;
    fail = true;
  }
  EXPECT_EQ(fail, false);
}




TEST(ADProvenanceDBclientTest, SendReceiveVectorAnomalyData){

  bool fail = false;
  ADProvenanceDBclient client(rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    std::vector<nlohmann::json> objs(2);
    objs[0]["hello"] = "world " + rank_str;
    objs[1]["hello"] = "again " + rank_str;

    std::cout << "Sending " << std::endl << objs[0].dump() << std::endl << objs[1].dump() << std::endl;
    std::vector<yk_id_t> rid = client.sendMultipleData(objs, ProvenanceDataType::AnomalyData);
    EXPECT_EQ(rid.size(), 2);

    for(int i=0;i<2;i++){    
      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, rid[i], ProvenanceDataType::AnomalyData), true );
    
      std::cout << "Testing retrieved anomaly data:" << check.dump() << std::endl;

      //NB, retrieval adds extra __id field, so objects not identical
      bool same = check["hello"] ==  (i==0? "world " + rank_str : "again " + rank_str);
      std::cout << "JSON objects are the same? " << same << std::endl;
      EXPECT_EQ(same, true);
    }
  }catch(const std::exception &ex){
    std::cout << "ERROR caught: " << ex.what() << std::endl;
    fail = true;
  }
  EXPECT_EQ(fail, false);
}

TEST(ADProvenanceDBclientTest, SendReceiveJSONarrayAnomalyData){

  bool fail = false;
  ADProvenanceDBclient client(rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    nlohmann::json objs = nlohmann::json::array();
    objs[0] = nlohmann::json::object({ {"hello","myfriend " + rank_str} });
    objs[1] = nlohmann::json::object({ {"hello","what? " + rank_str} });

    std::cout << "Sending " << std::endl << objs.dump() << std::endl;
    std::vector<yk_id_t> rid = client.sendMultipleData(objs, ProvenanceDataType::AnomalyData);
    EXPECT_EQ(rid.size(), 2);

    for(int i=0;i<2;i++){    
      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, rid[i], ProvenanceDataType::AnomalyData), true );
    
      std::cout << "Testing retrieved anomaly data:" << check.dump() << " with index " << rid[i] << std::endl;

      //NB, retrieval adds extra __id field, so objects not identical
      bool same = check["hello"] ==  (i==0? "myfriend " + rank_str : "what? " + rank_str);
      std::cout << "JSON objects are the same? " << same << std::endl;
      EXPECT_EQ(same, true);
    }
  }catch(const std::exception &ex){
    std::cout << "ERROR caught: " << ex.what() << std::endl;
    fail = true;
  }
  EXPECT_EQ(fail, false);
}

TEST(ADProvenanceDBclientTest, SendReceiveVectorAnomalyDataPacked){
  //Try the packed multisend
  bool fail = false;
  ADProvenanceDBclient client(rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);
    client.enablePackedMultiSends(true);

    std::vector<nlohmann::json> objs(2);
    objs[0]["hello"] = "world packed " + rank_str;
    objs[1]["hello"] = "again packed " + rank_str;

    std::cout << "Sending " << std::endl << objs[0].dump() << std::endl << objs[1].dump() << std::endl;
    std::vector<yk_id_t> rid = client.sendMultipleData(objs, ProvenanceDataType::AnomalyData);
    EXPECT_EQ(rid.size(), 2);

    for(int i=0;i<2;i++){    
      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, rid[i], ProvenanceDataType::AnomalyData), true );
    
      std::cout << "Testing retrieved anomaly data:" << check.dump() << std::endl;

      //NB, retrieval adds extra __id field, so objects not identical
      bool same = check["hello"] ==  (i==0? "world packed " + rank_str : "again packed " + rank_str);
      std::cout << "JSON objects are the same? " << same << std::endl;
      EXPECT_EQ(same, true);
    }

    //Try without RDMA
    client.enableMultiSendRDMA(false);

    objs[0]["hello"] = "world packed no-RDMA " + rank_str;
    objs[1]["hello"] = "again packed no-RDMA " + rank_str;

    std::cout << "Sending " << std::endl << objs[0].dump() << std::endl << objs[1].dump() << std::endl;
    rid = client.sendMultipleData(objs, ProvenanceDataType::AnomalyData);
    EXPECT_EQ(rid.size(), 2);

    for(int i=0;i<2;i++){    
      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, rid[i], ProvenanceDataType::AnomalyData), true );
    
      std::cout << "Testing retrieved anomaly data:" << check.dump() << std::endl;

      //NB, retrieval adds extra __id field, so objects not identical
      bool same = check["hello"] ==  (i==0? "world packed no-RDMA " + rank_str : "again no-RDMA " + rank_str);
      std::cout << "JSON objects are the same? " << same << std::endl;
      EXPECT_EQ(same, true);
    }

  }catch(const std::exception &ex){
    std::cout << "ERROR caught: " << ex.what() << std::endl;
    fail = true;
  }
  EXPECT_EQ(fail, false);
}





TEST(ADProvenanceDBclientTest, SendReceiveAnomalyDataAsync){

  bool fail = false;
  ADProvenanceDBclient client(rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    nlohmann::json obj;
    obj["hello"] = "world " + rank_str;
    std::cout << "Sending " << obj.dump() << " asynchronously" << std::endl;

    OutstandingRequest req;

    client.sendDataAsync(obj, ProvenanceDataType::AnomalyData, &req);

    req.wait(); //wait for completion
    int rid = req.ids[0];
    
    nlohmann::json check;
    EXPECT_EQ( client.retrieveData(check, rid, ProvenanceDataType::AnomalyData), true );
    
    std::cout << "Testing retrieved anomaly data:" << check.dump() << std::endl;

    //NB, retrieval adds extra __id field, so objects not identical
    bool same = check["hello"] == "world " + rank_str;
    std::cout << "JSON objects are the same? " << same << std::endl;

    EXPECT_EQ(same, true);
  }catch(const std::exception &ex){
    std::cout << "ERROR caught: " << ex.what() << std::endl;
    fail = true;
  }
  EXPECT_EQ(fail, false);
}


TEST(ADProvenanceDBclientTest, SendReceiveVectorAnomalyDataAsync){

  bool fail = false;
  ADProvenanceDBclient client(rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    std::vector<nlohmann::json> objs(2);
    objs[0]["hello"] = "world " + rank_str;
    objs[1]["hello"] = "again " + rank_str;

    OutstandingRequest req;
    
    std::cout << "Sending " << std::endl << objs[0].dump() << std::endl << objs[1].dump() << std::endl;
    client.sendMultipleDataAsync(objs, ProvenanceDataType::AnomalyData,&req);
    EXPECT_EQ(req.ids.size(), 2);

    req.wait(); //wait for completion
    
    for(int i=0;i<2;i++){    
      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, req.ids[i], ProvenanceDataType::AnomalyData), true );
    
      std::cout << "Testing retrieved anomaly data:" << check.dump() << std::endl;

      //NB, retrieval adds extra __id field, so objects not identical
      bool same = check["hello"] ==  (i==0? "world " + rank_str : "again " + rank_str);
      std::cout << "JSON objects are the same? " << same << std::endl;
      EXPECT_EQ(same, true);
    }
  }catch(const std::exception &ex){
    std::cout << "ERROR caught: " << ex.what() << std::endl;
    fail = true;
  }
  EXPECT_EQ(fail, false);
}



TEST(ADProvenanceDBclientTest, SendReceiveVectorAnomalyDataAsyncPacked){

  bool fail = false;
  ADProvenanceDBclient client(rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);
    client.enablePackedMultiSends(true);

    std::vector<nlohmann::json> objs(2);
    objs[0]["hello"] = "world async packed " + rank_str;
    objs[1]["hello"] = "again async packed " + rank_str;

    OutstandingRequest req;
    
    std::cout << "Sending " << std::endl << objs[0].dump() << std::endl << objs[1].dump() << std::endl;
    client.sendMultipleDataAsync(objs, ProvenanceDataType::AnomalyData,&req);
    EXPECT_EQ(req.ids.size(), 2);

    req.wait(); //wait for completion
    
    for(int i=0;i<2;i++){    
      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, req.ids[i], ProvenanceDataType::AnomalyData), true );
    
      std::cout << "Testing retrieved anomaly data:" << check.dump() << std::endl;

      //NB, retrieval adds extra __id field, so objects not identical
      bool same = check["hello"] ==  (i==0? "world async packed " + rank_str : "again async packed " + rank_str);
      std::cout << "JSON objects are the same? " << same << std::endl;
      EXPECT_EQ(same, true);
    }

    req = OutstandingRequest();
    client.enableMultiSendRDMA(false);

    objs[0]["hello"] = "world async packed no-RDMA " + rank_str;
    objs[1]["hello"] = "again async packed no-RDMA " + rank_str;

    std::cout << "Sending " << std::endl << objs[0].dump() << std::endl << objs[1].dump() << std::endl;
    client.sendMultipleDataAsync(objs, ProvenanceDataType::AnomalyData,&req);
    EXPECT_EQ(req.ids.size(), 2);

    req.wait(); //wait for completion
    
    for(int i=0;i<2;i++){    
      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, req.ids[i], ProvenanceDataType::AnomalyData), true );
    
      std::cout << "Testing retrieved anomaly data:" << check.dump() << std::endl;

      //NB, retrieval adds extra __id field, so objects not identical
      bool same = check["hello"] ==  (i==0? "world async packed no-RDMA " + rank_str : "again async packed no-RDMA " + rank_str);
      std::cout << "JSON objects are the same? " << same << std::endl;
      EXPECT_EQ(same, true);
    }


  }catch(const std::exception &ex){
    std::cout << "ERROR caught: " << ex.what() << std::endl;
    fail = true;
  }
  EXPECT_EQ(fail, false);
}





TEST(ADProvenanceDBclientTest, SendReceiveJSONarrayAnomalyDataAsync){

  bool fail = false;
  ADProvenanceDBclient client(rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    nlohmann::json objs = nlohmann::json::array();
    objs[0] = nlohmann::json::object({ {"hello","myfriend " + rank_str} });
    objs[1] = nlohmann::json::object({ {"hello","what? " + rank_str} });

    OutstandingRequest req;

    std::cout << "Sending " << std::endl << objs.dump() << std::endl;
    client.sendMultipleDataAsync(objs, ProvenanceDataType::AnomalyData, &req);
    EXPECT_EQ(req.ids.size(), 2);

    req.wait(); //wait for completion

    EXPECT_TRUE(req.req.test());

    for(int i=0;i<2;i++){    
      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, req.ids[i], ProvenanceDataType::AnomalyData), true );
    
      std::cout << "Testing retrieved anomaly data:" << check.dump() << " with index " << req.ids[i] << std::endl;

      //NB, retrieval adds extra __id field, so objects not identical
      bool same = check["hello"] ==  (i==0? "myfriend " + rank_str : "what? " + rank_str);
      std::cout << "JSON objects are the same? " << same << std::endl;
      EXPECT_EQ(same, true);
    }
  }catch(const std::exception &ex){
    std::cout << "ERROR caught: " << ex.what() << std::endl;
    fail = true;
  }
  EXPECT_EQ(fail, false);
}



//Test the async send that does not return an outstanding request
TEST(ADProvenanceDBclientTest, SendReceiveAnomalyDataAsyncAnonymous){

  bool fail = false;
  ADProvenanceDBclient client(rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    nlohmann::json obj;
    obj["hello"] = "world " + rank_str;
    std::cout << "Sending " << obj.dump() << " asynchronously" << std::endl;

    client.sendDataAsync(obj, ProvenanceDataType::AnomalyData);

    //Ensure there is an outstanding request in the manager (this list is not purged automatically even if the eventual has completed; purges are triggered through internal processes in the client but in this test we avoid those, hence we know the list should remain populated)
    auto & man = client.getAnomSendManager();
    EXPECT_EQ(man.getOutstandingEvents().size(), 1);

    //Check the eventual is satisfied in the background
    while(!man.getOutstandingEvents().front().test() ){
      std::cout << "Outstanding request not yet complete" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    std::cout << "Outstanding request complete" << std::endl;
    
    std::cout << "Waiting for outstanding sends to complete (should take no time now)" << std::endl;
    client.waitForSendComplete();

    //Check 0 outstanding sends
    EXPECT_EQ( client.getNoutstandingAsyncReqs(), 0 );

    std::vector<std::string> all_str = client.retrieveAllData(ProvenanceDataType::AnomalyData);
    bool found = false;
    for(auto &rs : all_str){
      nlohmann::json rc = nlohmann::json::parse(rs);
      if(rc["hello"] == "world " + rank_str){
	found =  true;
	break;
      }
    }
    EXPECT_TRUE(found);
  }catch(const std::exception &ex){
    std::cout << "ERROR caught: " << ex.what() << std::endl;
    fail = true;
  }
  EXPECT_EQ(fail, false);
}


//Test that multiple submitted async outstanding requests drains naturally
TEST(ADProvenanceDBclientTest, SendReceiveAnomalyDataAsyncAnonymousDrains){

  bool fail = false;
  ADProvenanceDBclient client(rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    nlohmann::json obj;
    obj["hello"] = "world " + rank_str;
    std::cout << "Sending " << obj.dump() << " asynchronously" << std::endl;

    int nsend = 50;
    for(int i=0;i<nsend;i++){
      client.sendDataAsync(obj, ProvenanceDataType::AnomalyData);
    }

    int no;
    while( (no=client.getNoutstandingAsyncReqs()) > 0){
      std::cout << "# outstanding " << no << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    EXPECT_EQ( client.getNoutstandingAsyncReqs(), 0 );

    std::cout << "Waiting for outstanding sends to complete" << std::endl;
    client.waitForSendComplete();
  }catch(const std::exception &ex){
    std::cout << "ERROR caught: " << ex.what() << std::endl;
    fail = true;
  }
  EXPECT_EQ(fail, false);
}







TEST(ADProvenanceDBclientTest, TestClearAll){

  bool fail = false;
  ADProvenanceDBclient client(rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    auto &coll = client.getCollection(ProvenanceDataType::AnomalyData);
   
    nlohmann::json objs = nlohmann::json::array();
    objs[0] = nlohmann::json::object({ {"hello","myfriend " + rank_str} });
    objs[1] = nlohmann::json::object({ {"hello","what? " + rank_str} });

    client.sendMultipleData(objs, ProvenanceDataType::AnomalyData);

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    EXPECT_NE(coll.size(), 0);

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    client.clearAllData(ProvenanceDataType::AnomalyData);

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    EXPECT_EQ(coll.size(), 0);
  }catch(const std::exception &ex){
    std::cout << "ERROR caught: " << ex.what() << std::endl;
    fail = true;
  }
  EXPECT_EQ(fail, false);
}


TEST(ADProvenanceDBclientTest, TestRetrieveAll){

  bool fail = false;
  ADProvenanceDBclient client(rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    auto &coll = client.getCollection(ProvenanceDataType::AnomalyData);

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    client.clearAllData(ProvenanceDataType::AnomalyData);

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    EXPECT_EQ(coll.size(), 0);

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif
  
    nlohmann::json objs = nlohmann::json::array();
    objs[0] = nlohmann::json::object({ {"rank", rank_str}, {"val", 0} });
    objs[1] = nlohmann::json::object({ {"rank", rank_str}, {"val", 1} });

    client.sendMultipleData(objs, ProvenanceDataType::AnomalyData);

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    std::vector<std::string> all = client.retrieveAllData(ProvenanceDataType::AnomalyData);
    EXPECT_GE(all.size(), 2); //other ranks may also have stored data
    
    std::vector<nlohmann::json> all_json(all.size());
    for(int i=0;i<all.size();i++) all_json[i] = nlohmann::json::parse(all[i]);

    int rcount = 0;
    int count0 = 0;
    int count1 = 0;

    for(int i=0;i<all.size();i++){
      EXPECT_EQ( all_json[i].contains("rank"), true );
      if(all_json[i]["rank"] == rank_str){
	++rcount;
	int val = all_json[i]["val"];
	if(val == 0) count0++;
	else if(val == 1) count1++;
	else FAIL() << "Invalid value " << val;
      }
    }
    EXPECT_EQ(rcount, 2);
    EXPECT_EQ(count0, 1);
    EXPECT_EQ(count1, 1);
  }catch(const std::exception &ex){
    std::cout << "ERROR caught: " << ex.what() << std::endl;
    fail = true;
  }
  EXPECT_EQ(fail, false);
}


TEST(ADProvenanceDBclientTest, TestLuaFilter){

  bool fail = false;
  ADProvenanceDBclient client(rank);
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectSingleServer(addr,nshards);

    auto &coll = client.getCollection(ProvenanceDataType::AnomalyData);

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    client.clearAllData(ProvenanceDataType::AnomalyData);

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    EXPECT_EQ(coll.size(), 0);

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif
  
    nlohmann::json objs = nlohmann::json::array();
    objs[0] = nlohmann::json::object({ {"rank", rank}, {"val", 0} });
    objs[1] = nlohmann::json::object({ {"rank", rank}, {"val", 1} });

    client.sendMultipleData(objs, ProvenanceDataType::AnomalyData);

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    std::string query = "data = cjson.decode(__doc__)\nreturn data[\"rank\"] == " + rank_str + " and data[\"val\"] == 0";
    std::vector<std::string> rdata = client.filterData(ProvenanceDataType::AnomalyData, query);
    EXPECT_EQ(rdata.size(),1);
    
    nlohmann::json rdata_json = nlohmann::json::parse(rdata[0]);
    EXPECT_EQ( rdata_json["rank"], rank );
    EXPECT_EQ( rdata_json["val"], 0 );

    rdata.clear();

    query = "data = cjson.decode(__doc__)\nreturn data[\"rank\"] == " + rank_str + " and data[\"val\"] == 1";
    rdata = client.filterData(ProvenanceDataType::AnomalyData, query);
    EXPECT_EQ(rdata.size(),1);
    
    rdata_json = nlohmann::json::parse(rdata[0]);
    EXPECT_EQ( rdata_json["rank"], rank );
    EXPECT_EQ( rdata_json["val"], 1 );
  }catch(const std::exception &ex){
    std::cout << "ERROR caught: " << ex.what() << std::endl;
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

#ifdef ENABLE_MARGO_STATE_DUMP
TEST(ADProvenanceDBclientTest, TestStateDump){
  if(rank == 0){
    std::regex fn(R"(margo_dump\.)");
    if(fileExistsMatchingRegex(".", fn))
      throw std::runtime_error("Existing file of form margo_dump.* found in test directory, delete this before running");

    bool fail = true;
    ADProvenanceDBclient client(rank);
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
  MPI_Comm_size(MPI_COMM_WORLD, &ranks);
#else
  rank = 0;
  ranks = 1;
#endif

  rank_str = anyToStr(rank);

  ::testing::InitGoogleTest(&argc, argv);
  int ret = RUN_ALL_TESTS();

#ifdef USE_MPI
  MPI_Finalize();
#endif
  return ret;
}
