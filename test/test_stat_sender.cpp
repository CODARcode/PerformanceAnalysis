#include <chimbuko_config.h>
#include "chimbuko/modules/performance_analysis/pserver/GlobalAnomalyStats.hpp"
#include "chimbuko/modules/performance_analysis/pserver/GlobalCounterStats.hpp"
#include "chimbuko/core/pserver/PSstatSender.hpp"
#include "chimbuko/modules/performance_analysis/ad/ADLocalFuncStatistics.hpp"
#include "chimbuko/modules/performance_analysis/ad/ADLocalCounterStatistics.hpp"
#include <gtest/gtest.h>
#ifdef USE_MPI
#include <mpi.h>
#endif
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

  ExecDataMap_t dmap;
  dmap[func_id].push_back(it1);
  dmap[func_id].push_back(it2);

  ADExecDataInterface iface(&dmap);
  {
    auto d = iface.getDataSet(0);
    d[0].label = d[1].label = ADDataInterface::EventType::Outlier;
    iface.recordDataSetLabels(d,0);
  }

  ADLocalFuncStatistics loc(pid,rid,0);
  loc.gatherStatistics(&dmap);
  loc.gatherAnomalies(iface);

  GlobalAnomalyStats glob;
  glob.add_anomaly_data(loc);

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

  ADLocalCounterStatistics cs(33,77, &which_counters);
  
  RunStats stats;
  for(int i=0;i<100;i++) stats.push(double(i));

  cs.setStats(counter, stats);
  
  GlobalCounterStats glob;
  glob.add_counter_data(cs);

  PSstatSender stat_sender(1000);
  int calls = 0;
  stat_sender.add_payload(new PSstatSenderGlobalCounterStatsCallbackPayload(&glob, &calls));
  
  stat_sender.run_stat_sender(url);
  std::this_thread::sleep_for(std::chrono::milliseconds(5500));    
  stat_sender.stop_stat_sender();  
  EXPECT_EQ(stat_sender.bad(), false);
  EXPECT_EQ(calls,6); //one immediately then 5 more
}

TEST(PSstatSenderTest, DiskWriteWorks)
{
  std::string outdir = ".";

  std::string counter = "mYcOuNtEr";

  std::unordered_set<std::string> which_counters = {counter};

  ADLocalCounterStatistics cs(33, 77, &which_counters);
  
  RunStats stats;
  for(int i=0;i<100;i++) stats.push(double(i));

  cs.setStats(counter, stats);
  
  GlobalCounterStats glob;
  glob.add_counter_data(cs);

  PSstatSenderGlobalCounterStatsPayload *payload = new PSstatSenderGlobalCounterStatsPayload(&glob);
  PSstatSender stat_sender(1000);
  stat_sender.add_payload(payload);
  
  stat_sender.run_stat_sender("",outdir);
  std::this_thread::sleep_for(std::chrono::milliseconds(5500));    
  stat_sender.stop_stat_sender();  
  EXPECT_EQ(stat_sender.bad(), false);

  std::ifstream in("pserver_output_stats_5.json");
  EXPECT_EQ(in.good(), true);
  nlohmann::json rd_data;
  in >> rd_data;

  nlohmann::json expect;
  payload->add_json(expect);
  
  EXPECT_EQ(rd_data , expect);
}









int main(int argc, char** argv)
{
    int result = 0;
    int provided;
    ::testing::InitGoogleTest(&argc, argv);

    chimbuko::enableVerboseLogging() = true;

#ifdef USE_MPI
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
#endif
    result = RUN_ALL_TESTS();
#ifdef USE_MPI
    MPI_Finalize();
#endif
    return result;
}
