#include<chimbuko/ad/ADLocalFuncStatistics.hpp>
#include<chimbuko/pserver/global_anomaly_stats.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"
#include<chimbuko/util/string.hpp>


using namespace chimbuko;

struct TestSetup{
  CallList_t fake_execs;
  ExecDataMap_t fake_exec_map;
  Anomalies anomalies;

  int pid;
  int rank;
  int thread;
  int nfuncs;
  int nevent;
  int nanomalies_per_func;

  RunStats outlier_scores;

  TestSetup(): pid(2), rank(1), thread(99), nfuncs(100), nevent(100), nanomalies_per_func(2), outlier_scores(true){
    for(int i=0;i<nfuncs;i++){
      for(int j=0;j<nevent;j++){
	ExecData_t e = createFuncExecData_t(pid, rank, thread,
					    i, "func"+anyToStr(i),
					    100*i, 100);
	double score = j + nevent*i;

	auto it = fake_execs.insert(fake_execs.end(),e);
	fake_exec_map[i].push_back(it);

	if(j<nanomalies_per_func){
	  it->set_outlier_score(score);
	  outlier_scores.push(score);	
	  anomalies.insert(it, Anomalies::EventType::Outlier);
	}

      }
    }
  }
};

TEST(ADLocalFuncStatisticsTest, Serialization){
  TestSetup setup;

  ADLocalFuncStatistics loc(setup.pid,setup.rank,0);
  loc.gatherStatistics(&setup.fake_exec_map);
  loc.gatherAnomalies(setup.anomalies);

  EXPECT_EQ( loc.getAnomalyData().get_outlier_score_stats().get_json().dump(), setup.outlier_scores.get_json().dump() );
  
  std::string ser = loc.net_serialize();

  ADLocalFuncStatistics rm;
  rm.net_deserialize(ser);

  EXPECT_EQ(loc, rm);
}

struct ADLocalFuncStatisticsTest: public ADLocalFuncStatistics{
  static std::pair<size_t, size_t> updateGlobalStatisticsTest(ADNetClient &net_client, const std::string &l_stats, int step){
    return ADLocalFuncStatistics::updateGlobalStatistics(net_client, l_stats, step);
  }
};

TEST(ADLocalFuncStatisticsTest, TestUpdateGlobalStatisticsWithPS){
#ifdef _USE_MPINET
#warning "Testing with MPINET not available"
#elif defined(_USE_ZMQNET)
  std::cout << "Using ZMQ net" << std::endl;

  Barrier barrier2(2);

  std::string sinterface = "tcp://*:5559";
  std::string sname = "tcp://localhost:5559";

  TestSetup setup;

  ADLocalFuncStatistics loc(setup.pid,setup.rank,0);
  loc.gatherStatistics(&setup.fake_exec_map);
  loc.gatherAnomalies(setup.anomalies);

  std::cout << "JSON size " << loc.get_json_state().dump().size() << std::endl;
  std::cout << "Binary size " << loc.get_state().serialize_cerealpb().size() << std::endl;


  GlobalAnomalyStats glob;

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
  
  std::string anom_stat_id = stringize("%d:%d", setup.pid, setup.rank);
  
  EXPECT_EQ( glob.get_n_anomaly_data(setup.pid, setup.rank), 1 ); //1 set of data added
  RunStats stats = glob.get_anomaly_stat_obj(setup.pid, setup.rank);
  EXPECT_EQ( stats.accumulate(), 200);

  nlohmann::json gdata = glob.collect();
  EXPECT_EQ( gdata["anomaly"].size(), 1 );
  EXPECT_EQ( gdata["anomaly"][0]["key"], anom_stat_id );
  EXPECT_EQ( gdata["anomaly"][0]["data"].size(), 1 );
  EXPECT_EQ( gdata["anomaly"][0]["data"][0].dump(), loc.getAnomalyData().get_json().dump() );
  EXPECT_EQ( gdata["anomaly"][0]["data"][0]["outlier_scores"].dump(), setup.outlier_scores.get_json().dump() );

  EXPECT_EQ( gdata["func"].size(), setup.nfuncs );
  bool found = false;
  for(int i=0;i<setup.nfuncs;i++){ //events are not in original order due to intermediate storing in unordered_map
    if( gdata["func"][i]["fid"] == 8 ){
      found = true;
      EXPECT_EQ( gdata["func"][i]["app"], setup.pid );
      EXPECT_EQ( gdata["func"][i]["name"], "func8" );
      std::cout << "Stats:" << gdata["func"][i]["stats"].dump() << std::endl;
      double c = gdata["func"][i]["stats"]["count"];
      EXPECT_EQ(c, 1.0); //1 step
      c = gdata["func"][i]["stats"]["accumulate"];
      EXPECT_EQ(c, (double)setup.nanomalies_per_func);
      break;
    }
  }
  EXPECT_EQ(found , true);


#else
#error "Requires compiling with MPI or ZMQ net"
#endif
}
