#ifdef CHIMBUKO_USE_MPINET
#include "chimbuko/net/mpi_net.hpp"
#include "chimbuko/param/sstd_param.hpp"
#include <gtest/gtest.h>
#include <mpi.h>

class MpiNetTest : public ::testing::Test
{
protected:

    virtual void SetUp()
    {

    }

    virtual void TearDown()
    {

    }
};

TEST_F(MpiNetTest, MpiNetEnvTest)
{
    chimbuko::MPINet net;
    net.init(nullptr, nullptr, 10);    
        
    ASSERT_EQ(MPI_THREAD_MULTIPLE, net.thread_level());
    ASSERT_EQ(1, net.size());
}

TEST_F(MpiNetTest, MpiNetSendRecvMultiThreadSingleClientTest)
{
    chimbuko::MPINet net;

    net.init(nullptr, nullptr, 10);    

    chimbuko::SstdParam param;
    net.set_parameter(&param, chimbuko::ParamKind::SSTD);
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
        EXPECT_EQ(n_clients*n_rolls*n_frames, param[funcid].N());
        EXPECT_NEAR(mean[i], param[funcid].mean(), mean[i]*0.01);
        EXPECT_NEAR(std[i], param[funcid].std(), std[i]*0.01);
    }
}

TEST_F(MpiNetTest, MpiNetSendRecvMultiThreadMultiClientTest)
{
    chimbuko::MPINet net;

    net.init(nullptr, nullptr, 10);    

    chimbuko::SstdParam param;
    net.set_parameter(&param, chimbuko::ParamKind::SSTD);
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
        EXPECT_EQ(n_clients*n_rolls*n_frames, param[funcid].N());
        EXPECT_NEAR(mean[i], param[funcid].mean(), mean[i]*0.01);
        EXPECT_NEAR(std[i], param[funcid].std(), std[i]*0.01);
    }
}

TEST_F(MpiNetTest, MpiNetSendRecvSingleThreadMultiClientTest)
{
    chimbuko::MPINet net;

    net.init(nullptr, nullptr, 1);    

    chimbuko::SstdParam param;
    net.set_parameter(&param, chimbuko::ParamKind::SSTD);
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
        EXPECT_EQ(n_clients*n_rolls*n_frames, param[funcid].N());
        EXPECT_NEAR(mean[i], param[funcid].mean(), mean[i]*0.01);
        EXPECT_NEAR(std[i], param[funcid].std(), std[i]*0.01);
    }
}

#endif