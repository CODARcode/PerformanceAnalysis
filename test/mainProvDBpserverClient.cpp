//Test the pserver client connection to the provdb
//Requires AD clients to be connected simulaneously. Do this by running the test with multiple ranks
//and have rank 0 do the tests and the other ranks just spawn AD clients
#include <chimbuko_config.h>
#include <gtest/gtest.h>
#include <cassert>
#include <chimbuko/core/ad/ADProvenanceDBclient.hpp>
#include <chimbuko/core/pserver/PSglobalProvenanceDBclient.hpp>
#include <chimbuko/core/verbose.hpp>
#include <chimbuko/core/util/string.hpp>
#include <chimbuko/modules/performance_analysis/provdb/ProvDBmoduleSetup.hpp>
#ifdef USE_MPI
#include <mpi.h>
#endif

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

//For these tests the provenance DB admin must be running
std::string addr;
int rank;
std::string rank_str;
ProvDBmoduleSetup setup;

TEST(PSglobalProvenanceDBclientTest, Connects){
  bool connect_fail = false;
  PSglobalProvenanceDBclient client(setup.getGlobalDBcollections());
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectServer(addr);
  }catch(const std::exception &ex){
    connect_fail = true;
  }
  EXPECT_EQ(connect_fail, false);
}

TEST(PSglobalProvenanceDBclientTest, SendReceiveData){

  bool fail = false;
  PSglobalProvenanceDBclient client(setup.getGlobalDBcollections());
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectServer(addr);

    {
      nlohmann::json obj;
      obj["hello"] = "world";
      std::cout << "Sending " << obj.dump() << std::endl;
      uint64_t rid = client.sendData(obj, "func_stats");
      EXPECT_NE(rid, -1);

      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, rid, "func_stats"), true );
      bool same = check["hello"] == "world";
      std::cout << "JSON objects are the same? " << same << std::endl;
      EXPECT_EQ(same, true);
    }
    {
      nlohmann::json obj;
      obj["what's"] = "up";
      std::cout << "Sending " << obj.dump() << std::endl;
      uint64_t rid = client.sendData(obj, "counter_stats");
      EXPECT_NE(rid, -1);

      nlohmann::json check;
      EXPECT_EQ( client.retrieveData(check, rid, "counter_stats"), true );
      bool same = check["what's"] == "up";
      std::cout << "JSON objects are the same? " << same << std::endl;
      EXPECT_EQ(same, true);
    }

  }catch(const std::exception &ex){
    fail = true;
  }
  EXPECT_EQ(fail, false);
}


TEST(PSglobalProvenanceDBclientTest, SendReceiveMultipleData){

  bool fail = false;
  PSglobalProvenanceDBclient client(setup.getGlobalDBcollections());
  std::cout << "Client attempting connection" << std::endl;
  try{
    client.connectServer(addr);

    {
      nlohmann::json obj = nlohmann::json::array();
      
      nlohmann::json o1;
      o1["hello"] = "world";

      nlohmann::json o2;
      o2["what's"] = "up";

      obj.push_back(o1);
      obj.push_back(o2);

      std::cout << "Sending " << obj.dump() << std::endl;
      std::vector<uint64_t> rid = client.sendMultipleData(obj, "func_stats");
      EXPECT_EQ(rid.size(),2);
      EXPECT_NE(rid[0], -1);
      EXPECT_NE(rid[1], -1);

      {
	nlohmann::json check;
	EXPECT_EQ( client.retrieveData(check, rid[0], "func_stats"), true );
	bool same = check["hello"] == "world";
	std::cout << "JSON objects are the same? " << same << std::endl;
	EXPECT_EQ(same, true);
      }

      {
	nlohmann::json check;
	EXPECT_EQ( client.retrieveData(check, rid[1], "func_stats"), true );
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
#ifdef USE_MPI
  MPI_Init(&argc, &argv);
#endif
  chimbuko::enableVerboseLogging() = true;

  assert(argc >= 2);
  addr = argv[1];

#ifdef USE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#else
  rank = 0;
#endif

  rank_str = anyToStr(rank);

  int ret;
  if(rank == 0){
    std::cout << "Rank 0 acting as pserver and connecting to database on " << addr << std::endl;
    ::testing::InitGoogleTest(&argc, argv);
    ret = RUN_ALL_TESTS();
#ifdef USE_MPI
    std::cout << "Rank 0 complete, waiting at barrier" << std::endl;
    MPI_Barrier(MPI_COMM_WORLD);
    std::cout << "Rank 0 passed barrier, exiting" << std::endl;
#endif
  }else{
    std::cout << "Rank " << rank << " acting as AD client and connecting to database on " << addr << std::endl;
    ADProvenanceDBclient client(setup.getMainDBcollections(),rank); 
    client.connectSingleServer(addr,1);    
#ifdef USE_MPI
    std::cout << "Rank " << rank << " waiting at barrier" << std::endl;
    MPI_Barrier(MPI_COMM_WORLD); //wait for rank 0 to do its business
    std::cout << "Rank " << rank << " passed barrier, exiting" << std::endl;
#endif
    ret = 0;
  }
#ifdef USE_MPI
  MPI_Finalize();
#endif  
  return ret;
}
