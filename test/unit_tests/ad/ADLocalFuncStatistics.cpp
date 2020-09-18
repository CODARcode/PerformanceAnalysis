#include<chimbuko/ad/ADLocalFuncStatistics.hpp>
#include<chimbuko/pserver/global_anomaly_stats.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"
#include<chimbuko/util/string.hpp>


using namespace chimbuko;

struct ADLocalFuncStatisticsTest: public ADLocalFuncStatistics{
  static std::pair<size_t, size_t> updateGlobalStatisticsTest(ADNetClient &net_client, const std::string &l_stats, int step){
    return ADLocalFuncStatistics::updateGlobalStatistics(net_client, l_stats, step);
  }
};


TEST(ADLocalFuncStatisticsTestUpdateGlobalStatisticsWithPS, Works){
#ifdef _USE_MPINET
#warning "Testing with MPINET not available"
#elif defined(_USE_ZMQNET)
  std::cout << "Using ZMQ net" << std::endl;

  Barrier barrier2(2);

  std::string sinterface = "tcp://*:5559";
  std::string sname = "tcp://localhost:5559";

  CallList_t fake_execs;
  ExecDataMap_t fake_exec_map;
  Anomalies anomalies;

  int rank = 0;
  int nfuncs=100;
  int nevent=100;
  int nanomalies_per_func = 2;
  for(int i=0;i<nfuncs;i++){
    for(int j=0;j<nevent;j++){
      ExecData_t e = createFuncExecData_t(0, rank, 0,
					  i, "func"+anyToStr(i),
					  100*i, 100);
      auto it = fake_execs.insert(fake_execs.end(),e);
      fake_exec_map[i].push_back(it);

      if(j<nanomalies_per_func){
	anomalies.insert(it, Anomalies::EventType::Outlier);
      }

    }
  }

  ADLocalFuncStatistics loc(0);
  loc.gatherStatistics(&fake_exec_map);
  loc.gatherAnomalies(anomalies);

  std::cout << "JSON size " << loc.get_json_state(0).dump().size() << std::endl;
  std::cout << "Binary size " << loc.get_state(0).serialize_cerealpb().size() << std::endl;


  GlobalAnomalyStats glob({1}); //expect 1 rank for program 0

  std::thread ps_thr([&]{
      int argc; char** argv;
      ZMQNet net;
      net.add_payload(new NetPayloadUpdateAnomalyStats(&glob));
      net.init(&argc, &argv, 1);
      net.run("");             
      net.finalize();
    });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		       
  std::thread out_thr([&]{
      std::cout << "AD thread waiting to connect" << std::endl;
      try{
	ADNetClient net_client;
	net_client.connect_ps(0, 0, sname);
	std::cout << "AD thread updating local stats" << std::endl;
	loc.updateGlobalStatistics(net_client);
	std::cout << "AD thread terminating connection" << std::endl;
	net_client.disconnect_ps();
      }catch(const std::exception &e){
	std::cerr << e.what() << std::endl;
      }
    });

  ps_thr.join();
  out_thr.join();

  
  EXPECT_EQ( glob.get_n_anomaly_data("0:0"), 1 ); //1 set of data added
  RunStats stats = glob.get_anomaly_stat_obj("0:0");
  EXPECT_EQ( stats.accumulate(), 200);

#else
#error "Requires compiling with MPI or ZMQ net"
#endif
}
