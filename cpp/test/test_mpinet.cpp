#ifdef CHIMBUKO_USE_MPINET
#include "chimbuko/net/mpi_net.hpp"
#include <gtest/gtest.h>
#include <mpi.h>

class MpiNetTest : public ::testing::Test
{
protected:
    chimbuko::MPINet * m_net;

    virtual void SetUp()
    {
        m_net = new chimbuko::MPINet();
        m_net->init(nullptr, nullptr, 10);
    }

    virtual void TearDown()
    {
        delete m_net;
    }
};

TEST_F(MpiNetTest, MpiNetEnvTest)
{
    ASSERT_EQ(MPI_THREAD_MULTIPLE, m_net->thread_level());
    ASSERT_EQ(1, m_net->size());
}

TEST_F(MpiNetTest, MpiNetSendRecvTest)
{
    m_net->init_parameter(chimbuko::ParamKind::SSTD);
    m_net->run();
    SUCCEED();
}

#endif