#include "chimbuko/pserver/global_anomaly_stats.hpp"
#include "chimbuko/pserver/global_counter_stats.hpp"
#include "chimbuko/pserver/PSstatSender.hpp"
#include "chimbuko/ad/ADLocalFuncStatistics.hpp"
#include "chimbuko/ad/ADLocalCounterStatistics.hpp"
#include <gtest/gtest.h>
#include <mpi.h>
#include <thread>
#include <chrono>
#include "unit_tests/unit_test_common.hpp"
#include <chimbuko/verbose.hpp>

using namespace chimbuko;

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


namespace chimbuko{
/**
 * @brief A payload used to test the data sent by CURL is correctly formatted
 */
struct PSstatSenderGlobalAnomalyStatsCallbackPayload: public PSstatSenderGlobalAnomalyStatsPayload{
  int *calls;
  PSstatSenderGlobalAnomalyStatsCallbackPayload(GlobalAnomalyStats *stats, int *_calls): PSstatSenderGlobalAnomalyStatsPayload(stats), calls(_calls){}

  bool do_fetch() const override{ return true; }
  void process_callback(const std::string &packet, const std::string &returned) const override{
    ++(*calls);
    std::cout << "CALLBACK PROCESS" << std::endl;
    std::cout << "PACKET   (size " << packet.size() << ") :\"" << packet << "\"" << std::endl;
    std::cout << "RETURNED (size " << returned.size() << ") :\"" << returned << "\"" << std::endl;
    nlohmann::json p = nlohmann::json::parse(packet);
    nlohmann::json r = nlohmann::json::parse(returned);

    if(p != r) throw std::runtime_error("test failed");
  }
};

}

TEST(PSstatSenderTest, StatSenderGlobalAnomalyStatsBounce)
{
  std::string url = "http://0.0.0.0:5000/bouncejson";

  unsigned long pid=0, rid=1, tid=2, func_id=3;
  std::string func_name = "my_func";

  CallList_t call_list;
  auto it1 = call_list.insert(call_list.end(), createFuncExecData_t(pid,rid,tid,func_id,func_name, 100, 200) );
  auto it2 = call_list.insert(call_list.end(), createFuncExecData_t(pid,rid,tid,func_id,func_name, 300, 400) );
  Anomalies anom;
  anom.insert(it1);
  anom.insert(it2);

  ExecDataMap_t dmap;
  dmap[func_id].push_back(it1);
  dmap[func_id].push_back(it2);

  ADLocalFuncStatistics loc(0);
  loc.gatherStatistics(&dmap);
  loc.gatherAnomalies(anom);

  GlobalAnomalyStats glob(std::vector<int>(1,2)); //so rid=1 makes sense
  glob.add_anomaly_data(loc.get_json_state(rid).dump());

  PSstatSender stat_sender;
  int calls = 0;
  stat_sender.add_payload(new PSstatSenderGlobalAnomalyStatsCallbackPayload(&glob, &calls));
  
  stat_sender.run_stat_sender(url);
  std::this_thread::sleep_for(std::chrono::seconds(5));    
  stat_sender.stop_stat_sender();  
  EXPECT_EQ(stat_sender.bad(), false);
  EXPECT_EQ(calls,1); //after the first call the internal data are deleted, so no more data should be sent despite multiple further sends
}




namespace chimbuko{
/**
 * @brief A payload used to test the data sent by CURL is correctly formatted
 */
struct PSstatSenderGlobalCounterStatsCallbackPayload: public PSstatSenderGlobalCounterStatsPayload{
  int *calls;
  PSstatSenderGlobalCounterStatsCallbackPayload(GlobalCounterStats *stats, int *_calls): PSstatSenderGlobalCounterStatsPayload(stats), calls(_calls){}

  bool do_fetch() const override{ return true; }
  void process_callback(const std::string &packet, const std::string &returned) const override{
    ++(*calls);
    std::cout << "CALLBACK PROCESS" << std::endl;
    std::cout << "PACKET   (size " << packet.size() << ") :\"" << packet << "\"" << std::endl;
    std::cout << "RETURNED (size " << returned.size() << ") :\"" << returned << "\"" << std::endl;
    nlohmann::json p = nlohmann::json::parse(packet);
    nlohmann::json r = nlohmann::json::parse(returned);

    if(p != r) throw std::runtime_error("test failed");
  }
};

}

TEST(PSstatSenderTest, StatSenderGlobalCounterStatsBounce)
{
  std::string url = "http://0.0.0.0:5000/bouncejson";

  std::string counter = "mYcOuNtEr";

  std::unordered_set<std::string> which_counters = {counter};

  ADLocalCounterStatistics cs(77, &which_counters);
  
  RunStats stats;
  for(int i=0;i<100;i++) stats.push(double(i));

  cs.setStats(counter, stats);
  
  GlobalCounterStats glob;
  glob.add_data(cs.get_json_state().dump());

  PSstatSender stat_sender(1000);
  int calls = 0;
  stat_sender.add_payload(new PSstatSenderGlobalCounterStatsCallbackPayload(&glob, &calls));
  
  stat_sender.run_stat_sender(url);
  std::this_thread::sleep_for(std::chrono::milliseconds(5500));    
  stat_sender.stop_stat_sender();  
  EXPECT_EQ(stat_sender.bad(), false);
  EXPECT_EQ(calls,6); //one immediately then 5 more
}









int main(int argc, char** argv)
{
    int result = 0;
    int provided;
    ::testing::InitGoogleTest(&argc, argv);

    Verbose::set_verbose(true);

    // for (int i = 1; i < argc; i++)
    //     printf("arg %2d = %s\n", i, argv[i]);

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    result = RUN_ALL_TESTS();
    MPI_Finalize();
    return result;
}
