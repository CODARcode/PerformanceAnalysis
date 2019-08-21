#ifdef _USE_MPINET
#include "chimbuko/net/mpi_net.hpp"
#else
#include "chimbuko/net/zmq_net.hpp"
#endif
#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/ad/AnomalyStat.hpp"
#include <gtest/gtest.h>
#include <mpi.h>

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
    chimbuko::MPINet net;
    ASSERT_EQ(MPI_THREAD_MULTIPLE, net.thread_level());
    ASSERT_EQ(1, net.size());
#else
    chimbuko::ZMQNet net;
    SUCCEED();
#endif
}

TEST_F(NetTest, NetSendRecvMultiThreadSingleClientTest)
{
#ifdef _USE_MPINET
    chimbuko::MPINet net;
#else
    chimbuko::ZMQNet net;
#endif

    chimbuko::SstdParam param;

    net.set_parameter( dynamic_cast<chimbuko::ParamInterface*>(&param) );

    net.init(nullptr, nullptr, 10);
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
    chimbuko::MPINet net;
#else
    chimbuko::ZMQNet net;
#endif

    chimbuko::SstdParam param;

    net.set_parameter( dynamic_cast<chimbuko::ParamInterface*>(&param) );
    net.init(nullptr, nullptr, 10);
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
    chimbuko::MPINet net;
#else
    chimbuko::ZMQNet net;
#endif

    chimbuko::SstdParam param;

    net.set_parameter( dynamic_cast<chimbuko::ParamInterface*>(&param) );
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
    using namespace chimbuko;

#ifdef _USE_MPINET
    chimbuko::MPINet net;
#else
    chimbuko::ZMQNet net;
#endif
    const int N_MPI_PROCESSORS = 10;

    chimbuko::SstdParam param({N_MPI_PROCESSORS});

    net.set_parameter( dynamic_cast<chimbuko::ParamInterface*>(&param) );
    net.init(nullptr, nullptr, 10);
    net.run();

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
        std::string stat_id = "0:" + std::to_string(rank);

        std::string stat_binary = param.get_anomaly_stat(stat_id);
        ASSERT_GT(stat_binary.size(), 0);
        
        size_t n = param.get_n_anomaly_data(stat_id);
        EXPECT_EQ(MAX_STEPS, (int)n);

        RunStats stats = RunStats::from_binary_state(stat_binary);
        EXPECT_NEAR(means[rank], stats.mean(), means[rank]*0.1);
        EXPECT_NEAR(stddevs[rank], stats.stddev(), stddevs[rank]*0.1);
    }

#ifdef _USE_ZMQNET
    net.finalize();
#endif
}
