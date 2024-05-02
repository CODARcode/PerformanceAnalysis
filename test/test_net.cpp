#ifdef _USE_MPINET
#include "chimbuko/core/net/mpi_net.hpp"
#else
#include "chimbuko/core/net/zmq_net.hpp"
#endif
#include "chimbuko/core/param/sstd_param.hpp"
#include "chimbuko/ad/AnomalyData.hpp"
#include "chimbuko/pserver/GlobalAnomalyStats.hpp"
#include "chimbuko/pserver/PSstatSender.hpp"
#include <gtest/gtest.h>
#include <mpi.h>
#include <thread>
#include <chrono>


using namespace chimbuko;

class NetTest : public ::testing::Test
{
protected:

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

TEST_F(NetTest, NetEnvTest)
{
#ifdef _USE_MPINET
    MPINet net;
    ASSERT_EQ(MPI_THREAD_MULTIPLE, net.thread_level());
    ASSERT_EQ(1, net.size());
#else
    ZMQNet net;
    SUCCEED();
#endif
}

TEST_F(NetTest, NetSendRecvMultiThreadSingleClientTest)
{
#ifdef _USE_MPINET
    MPINet net;
#else
    ZMQNet net;
#endif

    SstdParam param;
    int nt = 10;
    for(int i=0;i<nt;i++){
      net.add_payload(new NetPayloadUpdateParams(&param),i);
      net.add_payload(new NetPayloadGetParams(&param),i);
    }

    std::cout << "NetSendRecvMultiThreadSingleClientTest payloads are:" << std::endl;
    net.list_payloads(std::cout);

    //net.set_parameter( dynamic_cast<ParamInterface*>(&param) );

    net.init(nullptr, nullptr, nt);
    net.run();

    const int n_clients = 1;
    const int n_functions = 4;
    const double mean[n_functions] = {1.0, 10.0, 100.0, 1000.0};
    const double std[n_functions] = {0.1, 1.0, 10.0, 100.0};
    const int n_rolls = 1000;
    const int n_frames = 100;

    for (int i = 0; i < n_functions; i++)
    {
        unsigned long funcid = (unsigned long)i;
        EXPECT_EQ(n_clients*n_rolls*n_frames, param[funcid].count());
        EXPECT_NEAR(mean[i], param[funcid].mean(), mean[i]*0.01);
        EXPECT_NEAR(std[i], param[funcid].stddev(), std[i]*0.01);
    }

#ifdef _USE_ZMQNET
        net.finalize();
#endif
}

TEST_F(NetTest, NetSendRecvMultiThreadMultiClientTest)
{
#ifdef _USE_MPINET
    MPINet net;
#else
    ZMQNet net;
#endif

    SstdParam param;
    int nt = 10;
    for(int i=0;i<nt;i++){
      net.add_payload(new NetPayloadUpdateParams(&param),i);
      net.add_payload(new NetPayloadGetParams(&param),i);
    }
    net.init(nullptr, nullptr, nt);
    net.run();

    const int n_clients = 10;
    const int n_functions = 4;
    const double mean[n_functions] = {1.0, 10.0, 100.0, 1000.0};
    const double std[n_functions] = {0.1, 1.0, 10.0, 100.0};
    const int n_rolls = 1000;
    const int n_frames = 100;

    for (int i = 0; i < n_functions; i++)
    {
        unsigned long funcid = (unsigned long)i;
        EXPECT_EQ(n_clients*n_rolls*n_frames, param[funcid].count());
        EXPECT_NEAR(mean[i], param[funcid].mean(), mean[i]*0.01);
        EXPECT_NEAR(std[i], param[funcid].stddev(), std[i]*0.01);
    }

#ifdef _USE_ZMQNET
        net.finalize();
#endif
}

TEST_F(NetTest, NetSendRecvSingleThreadMultiClientTest)
{
#ifdef _USE_MPINET
    MPINet net;
#else
    ZMQNet net;
#endif

    SstdParam param;
    net.add_payload(new NetPayloadUpdateParams(&param));
    net.add_payload(new NetPayloadGetParams(&param));
    net.init(nullptr, nullptr, 1);
    net.run();

    const int n_clients = 10;
    const int n_functions = 4;
    const double mean[n_functions] = {1.0, 10.0, 100.0, 1000.0};
    const double std[n_functions] = {0.1, 1.0, 10.0, 100.0};
    const int n_rolls = 1000;
    const int n_frames = 100;

    for (int i = 0; i < n_functions; i++)
    {
        unsigned long funcid = (unsigned long)i;
        EXPECT_EQ(n_clients*n_rolls*n_frames, param[funcid].count());
        EXPECT_NEAR(mean[i], param[funcid].mean(), mean[i]*0.01);
        EXPECT_NEAR(std[i], param[funcid].stddev(), std[i]*0.01);
    }

#ifdef _USE_ZMQNET
        net.finalize();
#endif
}

TEST_F(NetTest, NetSendRecvAnomalyStatsTest)
{
#ifdef _USE_MPINET
    MPINet net;
#else
    ZMQNet net;
#endif
    const int N_MPI_PROCESSORS = 10;

    SstdParam param;
    GlobalAnomalyStats glob_stats;
    int nt = 10;
    for(int i=0;i<nt;i++){
      net.add_payload(new NetPayloadUpdateParams(&param),i);
      net.add_payload(new NetPayloadGetParams(&param),i);
      net.add_payload(new NetPayloadUpdateAnomalyStats(&glob_stats),i);
    }
    net.init(nullptr, nullptr, nt);
    net.run();

    std::cout << "check results" << std::endl;

    // check results (see pclient_stats.cpp)
    const std::vector<double> means = {
        100, 200, 300, 400, 500,
        150, 250, 350, 450, 550
    };
    const std::vector<double> stddevs = {
        10, 20, 30, 40, 50,
        15, 25, 35, 45, 55
    };
    const int MAX_STEPS = 1000;    

    for (int rank = 0; rank < N_MPI_PROCESSORS; rank++)
    {
        std::string strstats = glob_stats.get_anomaly_stat(0, rank);
        // std::cout << strstats << std::endl;
        ASSERT_GT(strstats.size(), 0);
        
        size_t n = glob_stats.get_n_anomaly_data(0, rank);
        // std::cout << n << std::endl;
        EXPECT_EQ(MAX_STEPS, (int)n);

        //RunStats stats = RunStats::from_strstate(strstate);
        //std::cout << stats.get_json().dump(2) << std::endl;
        nlohmann::json j = nlohmann::json::parse(strstats);
        EXPECT_NEAR(means[rank], j["mean"], means[rank]*0.1);
        EXPECT_NEAR(stddevs[rank], j["stddev"], stddevs[rank]*0.1);
    }

#ifdef _USE_ZMQNET
    net.finalize();
#endif
}

TEST_F(NetTest, NetStatSenderTest)
{
    PSstatSender stat_sender;
#ifdef _USE_MPINET
    MPINet net;
#else
    ZMQNet net;
#endif
    const int N_MPI_PROCESSORS = 10;

    SstdParam param;
    GlobalAnomalyStats glob_stats;
    nlohmann::json resp;

    stat_sender.add_payload(new PSstatSenderGlobalAnomalyStatsPayload(&glob_stats));
    stat_sender.run_stat_sender("http://0.0.0.0:5000/post");
    int nt = 10;
    for(int i=0;i<nt;i++){
      net.add_payload(new NetPayloadUpdateParams(&param),i);
      net.add_payload(new NetPayloadGetParams(&param),i);
      net.add_payload(new NetPayloadUpdateAnomalyStats(&glob_stats),i);
    }
    net.init(nullptr, nullptr, nt);
    net.run();

    // at this point, all pseudo AD modules finished sending 
    // anomaly statistics data.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    stat_sender.stop_stat_sender(1000);
    
    // check results (see pclient_stats.cpp)
    const std::vector<double> means = {
        100, 200, 300, 400, 500,
        150, 250, 350, 450, 550
    };
    const std::vector<double> stddevs = {
        10, 20, 30, 40, 50,
        15, 25, 35, 45, 55
    };
    // const int MAX_STEPS = 1000;    

    EXPECT_EQ(0, glob_stats.collect_stat_data().size());

    for (int rank = 0; rank < N_MPI_PROCESSORS; rank++)
    {
      std::string strstats = glob_stats.get_anomaly_stat(0, rank);
      ASSERT_GT(strstats.size(), 0);
      
      // As we send all anomaly data to the web server, 
      // it should be zero!
      size_t n = glob_stats.get_n_anomaly_data(0, rank);
      EXPECT_EQ(0, (int)n);
      
      nlohmann::json j = nlohmann::json::parse(strstats);
      EXPECT_NEAR(means[rank], j["mean"], means[rank]*0.1);
      EXPECT_NEAR(stddevs[rank], j["stddev"], stddevs[rank]*0.1);
    }    

#ifdef _USE_ZMQNET
    net.finalize();
#endif
}
