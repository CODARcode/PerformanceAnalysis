//Test the pserver client connection to the provdb
//Requires AD clients to be connected simulaneously. Do this by running the test with multiple ranks
//and have rank 0 do the tests and the other ranks just spawn AD clients
#include <gtest/gtest.h>
#include <cassert>
#include <chimbuko/ad/ADProvenanceDBclient.hpp>
#include <chimbuko/pserver/PSProvenanceDBclient.hpp>
#include <chimbuko/verbose.hpp>
#include <chimbuko/util/string.hpp>
#include <mpi.h>

using namespace chimbuko;

//For these tests the provenance DB admin must be running
std::string addr;
int rank;
std::string rank_str;

TEST(PSProvenanceDBclientTest, Connects){
  bool connect_fail = false;
  PSProvenanceDBclient client;
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connect(addr);
  }catch(const std::exception &ex){
    connect_fail = true;
  }
  EXPECT_EQ(connect_fail, false);
}

TEST(PSProvenanceDBclientTest, SendReceiveData){

  bool fail = false;
  PSProvenanceDBclient client;
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connect(addr);

    {
      nlohmann::json obj;
      obj["hello"] = "world";
      std::cout << "Sending " << obj.dump() << std::endl;
      uint64_t rid = client.sendData(obj, GlobalProvenanceDataType::FunctionStats);
      EXPECT_NE(rid, -1);

      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, rid, GlobalProvenanceDataType::FunctionStats), true );
      bool same = check["hello"] == "world";
      std::cout << "JSON objects are the same? " << same << std::endl;
      EXPECT_EQ(same, true);
    }
    {
      nlohmann::json obj;
      obj["what's"] = "up";
      std::cout << "Sending " << obj.dump() << std::endl;
      uint64_t rid = client.sendData(obj, GlobalProvenanceDataType::CounterStats);
      EXPECT_NE(rid, -1);

      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, rid, GlobalProvenanceDataType::CounterStats), true );
      bool same = check["what's"] == "up";
      std::cout << "JSON objects are the same? " << same << std::endl;
      EXPECT_EQ(same, true);
    }

  }catch(const std::exception &ex){
    fail = true;
  }
  EXPECT_EQ(fail, false);
}


TEST(PSProvenanceDBclientTest, SendReceiveMultipleData){

  bool fail = false;
  PSProvenanceDBclient client;
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connect(addr);

    {
      nlohmann::json obj = nlohmann::json::array();
      
      nlohmann::json o1;
      o1["hello"] = "world";

      nlohmann::json o2;
      o2["what's"] = "up";

      obj.push_back(o1);
      obj.push_back(o2);

      std::cout << "Sending " << obj.dump() << std::endl;
      std::vector<uint64_t> rid = client.sendMultipleData(obj, GlobalProvenanceDataType::FunctionStats);
      EXPECT_EQ(rid.size(),2);
      EXPECT_NE(rid[0], -1);
      EXPECT_NE(rid[1], -1);

      {
	nlohmann::json check;
	EXPECT_EQ( client.retrieveData(check, rid[0], GlobalProvenanceDataType::FunctionStats), true );
	bool same = check["hello"] == "world";
	std::cout << "JSON objects are the same? " << same << std::endl;
	EXPECT_EQ(same, true);
      }

      {
	nlohmann::json check;
	EXPECT_EQ( client.retrieveData(check, rid[1], GlobalProvenanceDataType::FunctionStats), true );
	bool same = check["what's"] == "up";
	std::cout << "JSON objects are the same? " << same << std::endl;
	EXPECT_EQ(same, true);
      }
    }

  }catch(const std::exception &ex){
    fail = true;
  }
  EXPECT_EQ(fail, false);
}



int main(int argc, char** argv) 
{
  MPI_Init(&argc, &argv);
  chimbuko::enableVerboseLogging() = true;

  assert(argc >= 2);
  addr = argv[1];

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  rank_str = anyToStr(rank);

  int ret;
  if(rank == 0){
    std::cout << "Rank 0 acting as pserver and connecting to database on " << addr << std::endl;
    ::testing::InitGoogleTest(&argc, argv);
    ret = RUN_ALL_TESTS();
    std::cout << "Rank 0 complete, waiting at barrier" << std::endl;
    MPI_Barrier(MPI_COMM_WORLD);
  }else{
    std::cout << "Rank " << rank << " acting as AD client and connecting to database on " << addr << std::endl;
    ADProvenanceDBclient client(rank); 
    client.connectSingleServer(addr,1);
    std::cout << "Rank " << rank << " waiting at barrier" << std::endl;
    MPI_Barrier(MPI_COMM_WORLD); //wait for rank 0 to do its business
    ret = 0;
  }
  MPI_Finalize();
  return ret;
}
