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

    //const int N_RANKS = 4;
    const std::vector<int> N_STEPS{6, 6, 6, 6};
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
        {134, 65, 83, 73, 77, 68, 34},
        { 89, 44, 62, 58, 54, 47, 39},
        { 99, 56, 59, 64, 66, 58, 43},
        {106, 58, 79, 49, 54, 66, 48}
    };


    std::string data_dir = "./data";
    std::string output_dir = "./data";
    std::string inputFile = "tau-metrics-" + std::to_string(world_rank) + ".bp";
    std::string engineType = "BPFile";

    double sigma = 6.0;

    Chimbuko driver;
    int step;
    unsigned long n_outliers = 0, frames = 0;
    unsigned long long n_func_events = 0, n_comm_events = 0, n_counter_events = 0;

    try{
      driver.init_io(world_rank, IOMode::Both, "", "", 0);
      driver.init_parser(data_dir, inputFile, engineType);
      driver.init_event();
      driver.init_counter();
      driver.init_outlier(world_rank, sigma);
    }catch(const std::exception &e){
      std::cerr << "Caught exception during driver init: " << e.what() << std::endl;
      throw e;
    }

      
    while ( driver.get_status() )
    {
        n_func_events = 0;
        n_comm_events = 0;
	n_counter_events = 0;
        n_outliers = 0;

	try{
	  driver.run(
		     world_rank, 
		     n_func_events, 
		     n_comm_events,
		     n_counter_events,
		     n_outliers, 
		     frames,
#ifdef _PERF_METRIC
		     "",
		     0,
#endif
		     true,
		     0
		     );
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
    }
    EXPECT_EQ(N_STEPS[world_rank], step);

    MPI_Barrier(MPI_COMM_WORLD);
    driver.finalize();
}

TEST_F(ADTest, BpfileWithNetTest)
{
    using namespace chimbuko;

    //const int N_RANKS = 4;
    const std::vector<int> N_STEPS{6, 6, 6, 6};
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
        {134, 50, 65, 54, 62, 54, 31},
        { 75, 20, 33, 42, 32, 47, 45},
        { 72, 34, 27, 25, 28, 28, 36},
        { 76, 32, 47, 26, 26, 32, 27}
    };


    std::string data_dir = "./data";
    std::string output_dir = "./data";
    std::string inputFile = "tau-metrics-" + std::to_string(world_rank) + ".bp";
    std::string engineType = "BPFile";
    std::string execOutput = "./temp";
#ifdef _PERF_METRIC
    std::string perfOutput = "./perf";
#endif
    double sigma = 6.0;

    Chimbuko driver;

    int step;
    unsigned long n_outliers = 0, frames = 0;
    unsigned long long n_func_events = 0, n_comm_events = 0, n_counter_events = 0;

    driver.init_io(world_rank, IOMode::Both, execOutput, "", 0);
    driver.init_parser(data_dir, inputFile, engineType);
    driver.init_event();
    driver.init_counter();
    driver.init_outlier(world_rank, sigma, "tcp://localhost:5559");

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

        driver.run(
            world_rank, 
            n_func_events, 
            n_comm_events,
	    n_counter_events,
            n_outliers, 
            frames,
#ifdef _PERF_METRIC
            perfOutput, 
            1,
#endif
            true,
            0
        );
        
        MPI_Send(&token, 1, MPI_INT, (world_rank+1)%world_size, 0, MPI_COMM_WORLD);
        if (world_rank == 0) {
            MPI_Recv(&token, 1, MPI_INT, world_size-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        if (driver.get_step() >= 0)
        {
            step = driver.get_step();

            // std::cout << "Rank: " << world_rank 
            //     << ", Step: " << step 
            //     << ", # func: " << n_func_events
            //     << ", # comm: " << n_comm_events
            //     << ", # outliers: " <<  n_outliers
            //     << std::endl;

	    //FIXME: Add expect for counter events
            EXPECT_EQ(N_FUNC[world_rank][step], n_func_events);
            EXPECT_EQ(N_COMM[world_rank][step], n_comm_events);
            EXPECT_EQ(N_OUTLIERS[world_rank][step], n_outliers);
        }
    }
    // std::cout << "Final, Rank: " << world_rank 
    //     << ", Step: " << step 
    //     << std::endl;
    EXPECT_EQ(N_STEPS[world_rank], step);

    MPI_Barrier(MPI_COMM_WORLD);
    driver.finalize();
}

TEST_F(ADTest, ReadExecDataTest)
{
    using namespace chimbuko;

    //const int N_RANKS = 4;
    const std::vector<int> N_STEPS{6, 6, 6, 6};

    const std::vector<std::vector<unsigned long>> N_OUTLIERS{
        {134, 50, 65, 54, 62, 54, 31},
        { 75, 20, 33, 42, 32, 47, 45},
        { 72, 34, 27, 25, 28, 28, 36},
        { 76, 32, 47, 26, 26, 32, 27}
    };

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
}
