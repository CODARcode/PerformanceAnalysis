#include <chimbuko_config.h>
#include <gtest/gtest.h>
#include <cassert>
#include <chimbuko/chimbuko.hpp>
#ifdef USE_MPI
#include<mpi.h>
#endif

using namespace chimbuko;

//For these tests the provenance DB admin must be running
int nshards;
int ninstances;
std::string addr_file_dir;
int world_rank;
int world_size;


TEST(ADTestWithProvDB, BpfileTest)
{
    ChimbukoParams params;
    params.rank = world_rank;

    params.trace_data_dir = "./data";
    params.trace_inputFile = "tau-metrics-" + std::to_string(world_rank) + ".bp";
    params.trace_engineType = "BPFile";

    params.prov_outputpath = ""; //don't output

    params.outlier_sigma = 6.0;
    params.only_one_frame = true; //just analyze first IO frame
    params.ad_algorithm = "sstd";

    params.provdb_addr_dir = addr_file_dir;
    params.nprovdb_shards = nshards;
    params.nprovdb_instances = ninstances;

    Chimbuko driver;
    unsigned long n_outliers = 0, frames = 0;
    unsigned long long n_func_events = 0, n_comm_events = 0, n_counter_events = 0;

    try{
      driver.initialize(params);
    }catch(const std::exception &e){
      std::cerr << "Caught exception during driver init: " << e.what() << std::endl;
      throw e;
    }
    
    EXPECT_EQ( driver.use_provdb(), true );


    //Just do one frame     
    try{
      driver.run(n_func_events, 
		 n_comm_events,
		 n_counter_events,
		 n_outliers, 
		 frames);
    }catch(const std::exception &e){
      std::cerr << "Caught exception in driver run: " << e.what() << std::endl;
      throw e;
    }
        
    //Wait for all outstanding sends to reach the database
    driver.getProvenanceDBclient().waitForSendComplete();

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
    assert( MPI_Allreduce(MPI_IN_PLACE, &n_outliers, 1, MPI_UNSIGNED_LONG , MPI_SUM, MPI_COMM_WORLD) == MPI_SUCCESS);
#endif

    //When using multiple server instances the outliers will be distributed across servers/ shards and won't all be accessible by the head node.
    //Thus we need to aggregate
    //Shards are assigned round-robin to ranks
    int rank_shard = world_rank % nshards;
    std::vector<unsigned long> noutliers_shard(nshards, 0);
    noutliers_shard[rank_shard] = driver.getProvenanceDBclient().retrieveAllData(ProvenanceDataType::AnomalyData).size();
   
    assert( MPI_Allreduce(MPI_IN_PLACE, noutliers_shard.data(), nshards, MPI_UNSIGNED_LONG , MPI_MAX, MPI_COMM_WORLD) == MPI_SUCCESS); //use max to fill in the array

    unsigned long noutliers_pdb = 0;
    for(int s=0;s<nshards;s++) noutliers_pdb += noutliers_shard[s];

    if(world_rank == 0) std::cout << "MPI rank 0: total outliers over all ranks: " << n_outliers << " number in database (over " << nshards << " shards): " << noutliers_pdb << std::endl;

    EXPECT_EQ(noutliers_pdb, n_outliers);

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif
}




int main(int argc, char** argv) 
{
  assert(argc == 4);
  addr_file_dir = argv[1];
  nshards = std::stoi(argv[2]);
  ninstances = std::stoi(argv[3]);
  std::cout << "Provenance DB admin address directory: " << addr_file_dir << " #shards: " << nshards << " #instances: " << ninstances << std::endl;   
  
#ifdef USE_MPI
  assert( MPI_Init(&argc, &argv) == MPI_SUCCESS );

  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
#else
  world_rank = 0; world_size = 1;
#endif

  ::testing::InitGoogleTest(&argc, argv);
  int ret = RUN_ALL_TESTS();

#ifdef USE_MPI
  assert(MPI_Finalize() == MPI_SUCCESS );
#endif  

  return ret;
}
