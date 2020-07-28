#include <gtest/gtest.h>
#include <cassert>
#include <chimbuko/chimbuko.hpp>

using namespace chimbuko;

//For these tests the provenance DB admin must be running
std::string addr;
int world_rank;
int world_size;


TEST(ADTestWithProvDB, BpfileTest)
{
    ChimbukoParams params;
    params.rank = world_rank;

    params.trace_data_dir = "./data";
    params.trace_inputFile = "tau-metrics-" + std::to_string(world_rank) + ".bp";
    params.trace_engineType = "BPFile";

    params.viz_iomode = IOMode::Both;
    params.viz_addr = "";
    params.viz_datadump_outputPath = "";

    params.outlier_sigma = 6.0;
    params.only_one_frame = true; //just analyze first IO frame
    
    params.provdb_addr = addr;

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

    MPI_Barrier(MPI_COMM_WORLD);

    assert( MPI_Allreduce(MPI_IN_PLACE, &n_outliers, 1, MPI_UNSIGNED_LONG , MPI_SUM, MPI_COMM_WORLD) == MPI_SUCCESS);

    if(world_rank == 0){
      std::cout << "MPI rank 0: total outliers over all ranks: " << n_outliers << std::endl;

      std::vector<std::string> recs = driver.getProvenanceDBclient().retrieveAllData(ProvenanceDataType::AnomalyData);
      std::cout << "MPI rank 0 retrieved " << recs.size() << " records from provenance database." << std::endl;
      EXPECT_EQ(recs.size(), n_outliers);
    }

    MPI_Barrier(MPI_COMM_WORLD);
}




int main(int argc, char** argv) 
{
  assert(argc == 2);
  addr = argv[1];
  std::cout << "Provenance DB admin is on address: " << addr << std::endl;

  assert( MPI_Init(&argc, &argv) == MPI_SUCCESS );

  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();

  assert(MPI_Finalize() == MPI_SUCCESS );
}
