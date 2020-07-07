#include <gtest/gtest.h>
#include <cassert>
#include <chimbuko/ad/ADProvenanceDBclient.hpp>
#include <chimbuko/verbose.hpp>

using namespace chimbuko;

//For these tests the provenance DB admin must be running
std::string addr;

TEST(ADProvenanceDBclientTest, Connects){

  bool connect_fail = false;
  ADProvenanceDBclient client;
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connect(addr);
  }catch(const std::exception &ex){
    connect_fail = true;
  }
  EXPECT_EQ(connect_fail, false);
}

TEST(ADProvenanceDBclientTest, ConnectsTwice){

  bool connect_fail = false;
  ADProvenanceDBclient client;
  std::cout << "Client attempting connection for second time" << std::endl;
  try{
    client.connect(addr);
  }catch(const std::exception &ex){
    connect_fail = true;
  }
  EXPECT_EQ(connect_fail, false);
}

TEST(ADProvenanceDBclientTest, SendReceiveAnomalyData){

  bool fail = false;
  ADProvenanceDBclient client;
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connect(addr);

    nlohmann::json obj;
    obj["hello"] = "world";
    std::cout << "Sending " << obj.dump() << std::endl;
    uint64_t rid = client.sendData(obj, ProvenanceDataType::AnomalyData);
    EXPECT_NE(rid, -1);
    
    nlohmann::json check;
    EXPECT_EQ( client.retrieveData(check, rid, ProvenanceDataType::AnomalyData), true );
    
    std::cout << "Testing retrieved anomaly data:" << check.dump() << std::endl;

    //NB, retrieval adds extra __id field, so objects not identical
    bool same = check["hello"] == "world";
    std::cout << "JSON objects are the same? " << same << std::endl;

    EXPECT_EQ(same, true);
  }catch(const std::exception &ex){
    fail = true;
  }
  EXPECT_EQ(fail, false);
}


TEST(ADProvenanceDBclientTest, SendReceiveMetadata){

  bool fail = false;
  ADProvenanceDBclient client;
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connect(addr);

    nlohmann::json obj;
    obj["hello"] = "world";
    std::cout << "Sending " << obj.dump() << std::endl;
    uint64_t rid = client.sendData(obj, ProvenanceDataType::Metadata);
    EXPECT_NE(rid, -1);
    
    nlohmann::json check;
    EXPECT_EQ( client.retrieveData(check, rid, ProvenanceDataType::Metadata), true );
    
    std::cout << "Testing retrieved metadata:" << check.dump() << std::endl;

    //NB, retrieval adds extra __id field, so objects not identical
    bool same = check["hello"] == "world";
    std::cout << "JSON objects are the same? " << same << std::endl;

    EXPECT_EQ(same, true);
  }catch(const std::exception &ex){
    fail = true;
  }
  EXPECT_EQ(fail, false);
}

TEST(ADProvenanceDBclientTest, SendReceiveVectorAnomalyData){

  bool fail = false;
  ADProvenanceDBclient client;
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connect(addr);

    std::vector<nlohmann::json> objs(2);
    objs[0]["hello"] = "world";
    objs[1]["hello"] = "again";

    std::cout << "Sending " << std::endl << objs[0].dump() << std::endl << objs[1].dump() << std::endl;
    std::vector<uint64_t> rid = client.sendMultipleData(objs, ProvenanceDataType::AnomalyData);
    EXPECT_EQ(rid.size(), 2);

    for(int i=0;i<2;i++){    
      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, rid[i], ProvenanceDataType::AnomalyData), true );
    
      std::cout << "Testing retrieved anomaly data:" << check.dump() << std::endl;

      //NB, retrieval adds extra __id field, so objects not identical
      bool same = check["hello"] ==  (i==0? "world" : "again");
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
  ADProvenanceDBclient client;
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connect(addr);

    nlohmann::json objs = nlohmann::json::array();
    objs[0] = nlohmann::json::object({ {"hello","myfriend"} });
    objs[1] = nlohmann::json::object({ {"hello","what?"} });

    std::cout << "Sending " << std::endl << objs.dump() << std::endl;
    std::vector<uint64_t> rid = client.sendMultipleData(objs, ProvenanceDataType::AnomalyData);
    EXPECT_EQ(rid.size(), 2);

    for(int i=0;i<2;i++){    
      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, rid[i], ProvenanceDataType::AnomalyData), true );
    
      std::cout << "Testing retrieved anomaly data:" << check.dump() << " with index " << rid[i] << std::endl;

      //NB, retrieval adds extra __id field, so objects not identical
      bool same = check["hello"] ==  (i==0? "myfriend" : "what?");
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
  ADProvenanceDBclient client;
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connect(addr);

    nlohmann::json obj;
    obj["hello"] = "world";
    std::cout << "Sending " << obj.dump() << " asynchronously" << std::endl;

    OutstandingRequest req;

    client.sendDataAsync(obj, ProvenanceDataType::AnomalyData, &req);

    req.wait(); //wait for completion
    int rid = req.ids[0];
    
    nlohmann::json check;
    EXPECT_EQ( client.retrieveData(check, rid, ProvenanceDataType::AnomalyData), true );
    
    std::cout << "Testing retrieved anomaly data:" << check.dump() << std::endl;

    //NB, retrieval adds extra __id field, so objects not identical
    bool same = check["hello"] == "world";
    std::cout << "JSON objects are the same? " << same << std::endl;

    EXPECT_EQ(same, true);
  }catch(const std::exception &ex){
    fail = true;
  }
  EXPECT_EQ(fail, false);
}


TEST(ADProvenanceDBclientTest, SendReceiveVectorAnomalyDataAsync){

  bool fail = false;
  ADProvenanceDBclient client;
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connect(addr);

    std::vector<nlohmann::json> objs(2);
    objs[0]["hello"] = "world";
    objs[1]["hello"] = "again";

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
      bool same = check["hello"] ==  (i==0? "world" : "again");
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
  ADProvenanceDBclient client;
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connect(addr);

    nlohmann::json objs = nlohmann::json::array();
    objs[0] = nlohmann::json::object({ {"hello","myfriend"} });
    objs[1] = nlohmann::json::object({ {"hello","what?"} });

    OutstandingRequest req;

    std::cout << "Sending " << std::endl << objs.dump() << std::endl;
    client.sendMultipleDataAsync(objs, ProvenanceDataType::AnomalyData, &req);
    EXPECT_EQ(req.ids.size(), 2);

    req.wait(); //wait for completion

    for(int i=0;i<2;i++){    
      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, req.ids[i], ProvenanceDataType::AnomalyData), true );
    
      std::cout << "Testing retrieved anomaly data:" << check.dump() << " with index " << req.ids[i] << std::endl;

      //NB, retrieval adds extra __id field, so objects not identical
      bool same = check["hello"] ==  (i==0? "myfriend" : "what?");
      std::cout << "JSON objects are the same? " << same << std::endl;
      EXPECT_EQ(same, true);
    }
  }catch(const std::exception &ex){
    fail = true;
  }
  EXPECT_EQ(fail, false);
}



int main(int argc, char** argv) 
{
  Verbose::set_verbose(true);
  assert(argc == 2);
  addr = argv[1];
  std::cout << "Provenance DB admin is on address: " << addr << std::endl;

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
