#include "chimbuko/pserver/global_anomaly_stats.hpp"
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

using namespace chimbuko;


//Check that it properly catches and handles errors
TEST(PSstatSenderTest, StatSenderPseudoTestForceFalse)
{
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
  std::string url = "http://0.0.0.0:5000/bouncejson";

  PSstatSender stat_sender;
  stat_sender.add_payload(new PSstatSenderTestCallbackPayload(false));
  
  stat_sender.run_stat_sender(url);
  std::this_thread::sleep_for(std::chrono::seconds(5));    
  stat_sender.stop_stat_sender();  
  EXPECT_EQ(stat_sender.bad(), false);
}




int main(int argc, char** argv)
{
    int result = 0;
    int provided;
    ::testing::InitGoogleTest(&argc, argv);

    // for (int i = 1; i < argc; i++)
    //     printf("arg %2d = %s\n", i, argv[i]);

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    result = RUN_ALL_TESTS();
    MPI_Finalize();
    return result;
}
