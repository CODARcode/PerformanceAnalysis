#ifdef _USE_MPINET
#include "chimbuko/net/mpi_net.hpp"
#else
#include "chimbuko/net/zmq_net.hpp"
#endif
#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/ad/AnomalyStat.hpp"
#include "chimbuko/global_anomaly_stats.hpp"
#include "chimbuko/pserver/PSstatSender.hpp"
#include <gtest/gtest.h>
#include <mpi.h>
#include <thread>
#include <chrono>


namespace chimbuko{
/**
 * @brief A payload used to test the data sent by CURL is correctly formatted
 */
struct PSstatSenderTestCallbackPayload: public PSstatSenderPayloadBase{
  bool m_force_nonmatch; //test the test by forcing a non-match
  PSstatSenderTestCallbackPayload(bool force_nonmatch = false): m_force_nonmatch(force_nonmatch){}

  void add_json(nlohmann::json &into) const override{ 
    static double num = 0;
    nlohmann::json j = {{"num", num}};
    num += 1.0;      
    into["test_packet"] = j;
  }
  bool do_fetch() const override{ return true; }
  void process_callback(const std::string &packet, const std::string &returned) const override{
    std::cout << "CALLBACK PROCESS" << std::endl;
    std::cout << "PACKET:" << packet << std::endl;
    std::cout << "RETURNED:" << returned << std::endl;

    nlohmann::json j_received = nlohmann::json::parse(returned);
    nlohmann::json j_expected = nlohmann::json::parse(packet);
    double drec = j_received["test_packet"]["num"].get<double>();
    double erec = j_expected["test_packet"]["num"].get<double>();
    if(std::abs(drec-erec) > 1e-6 || m_force_nonmatch){
      std::cout << "Expected:  " << erec << "\nReceived:  " << drec << std::endl;
      throw std::runtime_error("test failed!");
    }
  }
};

}

//Check that it properly catches and handles errors
TEST(PSstatSenderTest, StatSenderPseudoTestForceFalse)
{
  using namespace chimbuko;

  std::string url = "http://0.0.0.0:5000/bouncejson";

  PSstatSender stat_sender;
  stat_sender.add_payload(new PSstatSenderTestCallbackPayload(true));
  
  stat_sender.run_stat_sender(url);
  std::this_thread::sleep_for(std::chrono::seconds(5));    
  stat_sender.stop_stat_sender();  
  EXPECT_EQ(stat_sender.bad(), true);
}

TEST(PSstatSenderTest, StatSenderPseudoTestBounce)
{
  using namespace chimbuko;

  std::string url = "http://0.0.0.0:5000/bouncejson";

  PSstatSender stat_sender;
  stat_sender.add_payload(new PSstatSenderTestCallbackPayload(false));
  
  stat_sender.run_stat_sender(url);
  std::this_thread::sleep_for(std::chrono::seconds(5));    
  stat_sender.stop_stat_sender();  
  EXPECT_EQ(stat_sender.bad(), false);
}






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

    chimbuko::SstdParam param;
    chimbuko::GlobalAnomalyStats glob_stats({N_MPI_PROCESSORS});
    
    net.set_parameter( dynamic_cast<chimbuko::ParamInterface*>(&param) );
    net.set_global_anomaly_stats(&glob_stats);
    net.init(nullptr, nullptr, 10);
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
        std::string stat_id = "0:" + std::to_string(rank);
        // std::cout << stat_id << std::endl;

        std::string strstats = glob_stats.get_anomaly_stat(stat_id);
        // std::cout << strstats << std::endl;
        ASSERT_GT(strstats.size(), 0);
        
        size_t n = glob_stats.get_n_anomaly_data(stat_id);
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
    using namespace chimbuko;

    PSstatSender stat_sender;
#ifdef _USE_MPINET
    MPINet net;
#else
    ZMQNet net;
#endif
    const int N_MPI_PROCESSORS = 10;

    SstdParam param;
    GlobalAnomalyStats glob_stats({N_MPI_PROCESSORS});
    nlohmann::json resp;

    stat_sender.add_payload(new PSstatSenderGlobalAnomalyStatsPayload(&glob_stats));
    stat_sender.run_stat_sender("http://0.0.0.0:5000/post");

    net.set_parameter(dynamic_cast<ParamInterface*>(&param));
    net.set_global_anomaly_stats(&glob_stats);
    net.init(nullptr, nullptr, 10);
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
        std::string stat_id = "0:" + std::to_string(rank);

        std::string strstats = glob_stats.get_anomaly_stat(stat_id);
        ASSERT_GT(strstats.size(), 0);
        
        // As we send all anomaly data to the web server, 
        // it should be zero!
        size_t n = glob_stats.get_n_anomaly_data(stat_id);
        EXPECT_EQ(0, (int)n);

        nlohmann::json j = nlohmann::json::parse(strstats);
        EXPECT_NEAR(means[rank], j["mean"], means[rank]*0.1);
        EXPECT_NEAR(stddevs[rank], j["stddev"], stddevs[rank]*0.1);
    }    

#ifdef _USE_ZMQNET
    net.finalize();
#endif
}
