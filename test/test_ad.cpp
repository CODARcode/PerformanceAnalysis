#include "chimbuko/chimbuko.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

class ADTest : public ::testing::Test
{
protected:
    int world_rank, world_size;

    virtual void SetUp()
    {
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
        MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    }

    virtual void TearDown()
    {

    }

    template <typename K, typename V>
    void show_map(const std::unordered_map<K, V>& m)
    {
        for (const auto& it : m)
        {
            std::cout << it.first << " : " << it.second << std::endl;
        }
    }    
};

TEST_F(ADTest, EnvTest)
{
    EXPECT_EQ(4, world_size);
}

TEST_F(ADTest, BpfileTest)
{
    using namespace chimbuko;

    const std::vector<int> N_STEPS{7, 7, 7, 7};
    const std::vector<std::vector<size_t>> N_FUNC{
        {57077, 51845, 60561, 63278, 64628, 66484, 42233},
        {41215, 51375, 56620, 57683, 58940, 61010, 41963},
        {41108, 50820, 57237, 56590, 63458, 63931, 42486},
        {40581, 51127, 59175, 58158, 60465, 62516, 41238}
    }; 
    const std::vector<std::vector<size_t>> N_COMM{
        {89349, 107207, 121558, 123381, 128682, 131611, 83326},
        {78250, 94855 , 106301, 107222, 111581, 114273, 77817},
        {77020, 93355 , 105238, 106608, 114192, 118530, 79135},
        {77332, 93027 , 107856, 107628, 112019, 116468, 74517}
    };
    const std::vector<std::vector<unsigned long>> N_OUTLIERS{
      {137, 82, 94, 95, 90, 91, 51},
	{90, 60, 78, 75, 54, 68, 37},
	  {94, 79, 80, 87, 92, 92, 47},
	    {93, 81, 100, 81, 78, 85, 61}
    };

    ChimbukoParams params;
    params.rank = world_rank;

    params.trace_data_dir = "./data";
    params.trace_inputFile = "tau-metrics-" + std::to_string(world_rank) + ".bp";
    params.trace_engineType = "BPFile";

    params.prov_outputpath = "./temp"; //have to output somewhere if provdb not in use

    params.outlier_sigma = 6.0;
    params.only_one_frame = true; //just analyze first IO frame
    
    params.anom_win_size = 0;

    params.ad_algorithm = "sstd";

    Chimbuko driver;
    int step;
    unsigned long n_outliers = 0, frames = 0;
    unsigned long long n_func_events = 0, n_comm_events = 0, n_counter_events = 0;

    try{
      driver.initialize(params);
    }catch(const std::exception &e){
      std::cerr << "Caught exception during driver init: " << e.what() << std::endl;
      throw e;
    }

    std::vector<std::vector<unsigned long> > N_FUNC_ACTUAL(4);
    std::vector<std::vector<unsigned long> > N_COMM_ACTUAL(4);
    std::vector<std::vector<unsigned long> > N_OUTLIERS_ACTUAL(4);
      
    while ( driver.get_status() )
    {
        n_func_events = 0;
        n_comm_events = 0;
	n_counter_events = 0;
        n_outliers = 0;

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
	  
        if (driver.get_step() == -1)
            break;

        step = driver.get_step();

	//FIXME: Add an expect for the counter events
        EXPECT_EQ(N_FUNC[world_rank][step], n_func_events);
        EXPECT_EQ(N_COMM[world_rank][step], n_comm_events);
        EXPECT_EQ(N_OUTLIERS[world_rank][step], n_outliers);

	N_FUNC_ACTUAL[world_rank].push_back(n_func_events);
	N_COMM_ACTUAL[world_rank].push_back(n_comm_events);
	N_OUTLIERS_ACTUAL[world_rank].push_back(n_outliers);	    
    }

    int nstep = step+1;
    EXPECT_EQ(N_STEPS[world_rank], nstep);
    if(!world_rank) std::cout << "Steps performed: " << nstep << std::endl;

    MPI_Barrier(MPI_COMM_WORLD);

    //For gathering actual event counts, check 
    int nsteps_check[4] = {0,0,0,0};
    nsteps_check[world_rank] = nstep;
    
    MPI_Allreduce(MPI_IN_PLACE, nsteps_check, 4, MPI_INT, MPI_SUM, MPI_COMM_WORLD);    

    bool fail = false;
    for(int i=0;i<4;i++)
      if(nsteps_check[i] != nstep){
	fail = true;
	break;
      }
    
    if(!fail){
      //Fill in zeroes for other ranks
      for(int i=0;i<4;i++){
	if(i!=world_rank){
	  N_FUNC_ACTUAL[i].resize(nstep, 0);
	  N_COMM_ACTUAL[i].resize(nstep, 0);	  
	  N_OUTLIERS_ACTUAL[i].resize(nstep, 0);
	}
      }
      //Use global sum to sync all data to root
      for(int i=0;i<4;i++){
        MPI_Allreduce(MPI_IN_PLACE, N_FUNC_ACTUAL[i].data(), nstep, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);    
	MPI_Allreduce(MPI_IN_PLACE, N_COMM_ACTUAL[i].data(), nstep, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);    
	MPI_Allreduce(MPI_IN_PLACE, N_OUTLIERS_ACTUAL[i].data(), nstep, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);    
      }

      if(world_rank == 0){
	std::cout << "Actual value tables:" << std::endl;
	std::cout << "FUNC:" << std::endl;
	for(int i=0;i<4;i++){
	  for(int j=0;j<nstep;j++)
	    std::cout << N_FUNC_ACTUAL[i][j] << (j==nstep-1 ? "" :", ");
	  std::cout << std::endl;
	}
	std::cout << "COMM:" << std::endl;
	for(int i=0;i<4;i++){
	  for(int j=0;j<nstep;j++)
	    std::cout << N_COMM_ACTUAL[i][j] << (j==nstep-1 ? "" :", ");
	  std::cout << std::endl;
	}
	std::cout << "OUTLIERS:" << std::endl;
	for(int i=0;i<4;i++){
	  for(int j=0;j<nstep;j++)
	    std::cout << N_OUTLIERS_ACTUAL[i][j] << (j==nstep-1 ? "" :", ");
	  std::cout << std::endl;
	}
      }
    }
}

TEST_F(ADTest, BpfileWithNetTest)
{
    using namespace chimbuko;

    const std::vector<int> N_STEPS{7, 7, 7, 7};

    const std::vector<std::vector<size_t>> N_FUNC{
      {57077, 51845, 60561, 63278, 64628, 66484, 42233},
	{41215, 51375, 56620, 57683, 58940, 61010, 41963},
	  {41108, 50820, 57237, 56590, 63458, 63931, 42486},
	      {40581, 51127, 59175, 58158, 60465, 62516, 41238}
    };  //[rank][step]
    const std::vector<std::vector<size_t>> N_COMM{
      {89349, 107207, 121558, 123381, 128682, 131611, 83326},
	{78250, 94855, 106301, 107222, 111581, 114273, 77817},
	  {77020, 93355, 105238, 106608, 114192, 118530, 79135},
	    {77332, 93027, 107856, 107628, 112019, 116468, 74517}
    };  //[rank][step]
    const std::vector<std::vector<unsigned long>> N_OUTLIERS{
      {137, 52, 67, 71, 64, 74, 30},
	{102, 64, 85, 80, 55, 61, 33},
	  {76, 50, 56, 63, 83, 76, 52},
	    {77, 55, 94, 61, 49, 68, 33}
    };  //[rank][step]

    ChimbukoParams params;
    params.rank = world_rank;
    params.verbose = world_rank == 0;
    
    params.trace_data_dir = "./data";
    params.trace_inputFile = "tau-metrics-" + std::to_string(world_rank) + ".bp";
    params.trace_engineType = "BPFile";

    params.prov_outputpath = "./temp";

    params.anom_win_size = 0;

    params.pserver_addr = "tcp://localhost:5559"; //connect to the pserver

#ifdef _PERF_METRIC
    params.perf_outputpath =  "./perf";
    params.perf_step = 1;
#endif

    params.only_one_frame = true;
    params.outlier_sigma = 6.0;

    params.ad_algorithm = "sstd";

    Chimbuko driver;

    int step;
    unsigned long n_outliers = 0, frames = 0;
    unsigned long long n_func_events = 0, n_comm_events = 0, n_counter_events = 0;

    try{
      driver.initialize(params);
    }catch(const std::exception &e){
      std::cerr << "Caught exception during driver init: " << e.what() << std::endl;
      throw e;
    }

    std::vector<std::vector<unsigned long> > N_FUNC_ACTUAL(4);
    std::vector<std::vector<unsigned long> > N_COMM_ACTUAL(4);
    std::vector<std::vector<unsigned long> > N_OUTLIERS_ACTUAL(4);

    step = -1;
    while ( driver.get_status() )
    {
        n_func_events = 0;
        n_comm_events = 0;
        n_outliers = 0;

        // for the test purpose, we serialize the excution for run()
        int token = 1;
        if (world_rank != 0) {
            MPI_Recv(&token, 1, MPI_INT, world_rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

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
        
        MPI_Send(&token, 1, MPI_INT, (world_rank+1)%world_size, 0, MPI_COMM_WORLD);
        if (world_rank == 0) {
            MPI_Recv(&token, 1, MPI_INT, world_size-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        if (driver.get_step() >= 0)
        {
            step = driver.get_step();
	    if(!world_rank) std::cout << "Checking data for step " << step << std::endl;

	    //FIXME: Add expect for counter events
            EXPECT_EQ(N_FUNC[world_rank][step], n_func_events);
            EXPECT_EQ(N_COMM[world_rank][step], n_comm_events);
            EXPECT_EQ(N_OUTLIERS[world_rank][step], n_outliers);

	    N_FUNC_ACTUAL[world_rank].push_back(n_func_events);
	    N_COMM_ACTUAL[world_rank].push_back(n_comm_events);
	    N_OUTLIERS_ACTUAL[world_rank].push_back(n_outliers);
        }
    }
    int nstep = step+1;
    EXPECT_EQ(N_STEPS[world_rank], nstep);
    if(!world_rank) std::cout << "Steps performed: " << nstep << std::endl;

    MPI_Barrier(MPI_COMM_WORLD);

    //For gathering actual event counts, check 
    int nsteps_check[4] = {0,0,0,0};
    nsteps_check[world_rank] = nstep;
    
    MPI_Allreduce(MPI_IN_PLACE, nsteps_check, 4, MPI_INT, MPI_SUM, MPI_COMM_WORLD);    

    bool fail = false;
    for(int i=0;i<4;i++)
      if(nsteps_check[i] != nstep){
	fail = true;
	break;
      }
    
    if(!fail){
      //Fill in zeroes for other ranks
      for(int i=0;i<4;i++){
	if(i!=world_rank){
	  N_FUNC_ACTUAL[i].resize(nstep, 0);
	  N_COMM_ACTUAL[i].resize(nstep, 0);	  
	  N_OUTLIERS_ACTUAL[i].resize(nstep, 0);
	}
      }
      //Use global sum to sync all data to root
      for(int i=0;i<4;i++){
        MPI_Allreduce(MPI_IN_PLACE, N_FUNC_ACTUAL[i].data(), nstep, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);    
	MPI_Allreduce(MPI_IN_PLACE, N_COMM_ACTUAL[i].data(), nstep, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);    
	MPI_Allreduce(MPI_IN_PLACE, N_OUTLIERS_ACTUAL[i].data(), nstep, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);    
      }

      if(world_rank == 0){
	std::cout << "Actual value tables:" << std::endl;
	std::cout << "FUNC:" << std::endl;
	for(int i=0;i<4;i++){
	  for(int j=0;j<nstep;j++)
	    std::cout << N_FUNC_ACTUAL[i][j] << (j==nstep-1 ? "" :", ");
	  std::cout << std::endl;
	}
	std::cout << "COMM:" << std::endl;
	for(int i=0;i<4;i++){
	  for(int j=0;j<nstep;j++)
	    std::cout << N_COMM_ACTUAL[i][j] << (j==nstep-1 ? "" :", ");
	  std::cout << std::endl;
	}
	std::cout << "OUTLIERS:" << std::endl;
	for(int i=0;i<4;i++){
	  for(int j=0;j<nstep;j++)
	    std::cout << N_OUTLIERS_ACTUAL[i][j] << (j==nstep-1 ? "" :", ");
	  std::cout << std::endl;
	}
      }
    }
}

TEST_F(ADTest, ReadExecDataTest)
{
  //TODO: Update to check provenance data dump

#if 0

    using namespace chimbuko;

    const std::vector<int> N_STEPS{6, 6, 6, 6};

    const std::vector<std::vector<unsigned long>> N_OUTLIERS{
      {137, 52, 67, 71, 64, 74, 30},
	{102, 64, 85, 80, 55, 61, 33},
	  {76, 50, 56, 63, 83, 76, 52},
	    {77, 55, 94, 61, 49, 68, 33}
    };  //[rank][step]


    std::string execOutput = "temp/0/" + std::to_string(world_rank);

    for (int i = 0; i < N_STEPS[world_rank]; i++)
    {
        std::ifstream f;
        f.open(execOutput + "/" + std::to_string(i) + ".json");
        ASSERT_TRUE(f.is_open());

        nlohmann::json j;
        std::string s;

        f.seekg(0, std::ios::end);
        s.reserve(f.tellg());
        f.seekg(0, std::ios::beg);
        
        s.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        j = nlohmann::json::parse(s);

        // if (world_rank == 0 && i == 0)
        // {
        //     std::cout << j.dump(2) << std::endl;
        // }
        EXPECT_EQ(N_OUTLIERS[world_rank][i], j["exec"].size());        
    }
#endif
}
