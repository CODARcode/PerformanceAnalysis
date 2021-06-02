#include<chimbuko/ad/ADLocalCounterStatistics.hpp>
#include<chimbuko/pserver/global_counter_stats.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <random>

using namespace chimbuko;


TEST(ADLocalCounterStatisticsTest, GathersCorrectly){
  std::unordered_set<std::string> which_counters = {"MyCounter1", "MyCounter2"};

  unsigned long
    pid = 0,
    rid = 1,
    tid = 2,
    counter_id = 99;
  unsigned long value[4] = {999, 1002, 997, 1005};
  unsigned long ts[4] = {1234, 1235, 1236, 1237};
  std::string name = "MyCounter2"; //just one of the counters
  
  std::list<CounterData_t> counters;
  for(int i=0;i<4;i++) counters.push_back( createCounterData_t(pid,rid,tid, counter_id, value[i], ts[i], name) );

  CountersByIndex_t cntrs;
  for(auto it = counters.begin(); it != counters.end(); it++)
    cntrs[counter_id].push_back(it);
  
  ADLocalCounterStatistics cs(pid, 77, &which_counters);
  
  cs.gatherStatistics(cntrs);
  
  const auto &stats = cs.getStats();
  EXPECT_EQ(stats.size(), 1);
  EXPECT_EQ(stats.find(name), stats.begin());
  const RunStats &cstats = stats.begin()->second;
  
  RunStats expect;
  for(int i=0;i<4;i++)
    expect.push(value[i]);
  
  EXPECT_EQ(cstats, expect);
}


TEST(ADLocalCounterStatisticsTest, GlobalCounterStatsWorks){
  std::string nm = "MyCounter1";
  std::unordered_set<std::string> which_counters = {nm};

  unsigned long pid = 33;
  ADLocalCounterStatistics cs(pid, 0, &which_counters);

  RunStats a,b, sum;
  for(int i=0;i<100;i++){ a.push(double(i)); b.push(double(i)*3./2); }
  sum = a+b;

  GlobalCounterStats glob;

  cs.setStats(nm, a);
  glob.add_data_cerealpb(cs.get_state().serialize_cerealpb());
  
  auto gstats = glob.get_stats();
  auto pit = gstats.find(pid);
  EXPECT_NE( pit, gstats.end() );
  EXPECT_EQ( pit->second.count(nm), 1 );
  EXPECT_EQ( pit->second[nm], a );
  
  cs.setStats(nm, b);
  glob.add_data_cerealpb(cs.get_state().serialize_cerealpb());

  gstats = glob.get_stats();
  pit = gstats.find(pid);
  EXPECT_NE( pit, gstats.end() );
  EXPECT_EQ( pit->second.count(nm), 1 );
  EXPECT_EQ( pit->second[nm], sum );
}





struct ADLocalCounterStatisticsWrapper: public ADLocalCounterStatistics{
  static std::pair<size_t, size_t> updateGlobalStatisticsTest(ADNetClient &net_client, const std::string &l_stats, int step, int rank, std::string pserver_addr){
    return ADLocalCounterStatistics::updateGlobalStatistics(net_client, l_stats, step, rank, pserver_addr);
  }
};

TEST(ADLocalCounterStatisticsTest, UpdateGlobalStatisticsWithMockPS){
#ifdef _USE_MPINET
#warning "Testing with MPINET not available"
#elif defined(_USE_ZMQNET)
  std::cout << "Using ZMQ net" << std::endl;

  Barrier barrier2(2);

  std::string sinterface = "tcp://*:5559";
  std::string sname = "tcp://localhost:5559";

  std::thread ps_thr([&]{
		       MockParameterServer ps;
		       ps.start(barrier2, sinterface);
		       std::cout << "PS thread waiting at barrier" << std::endl;
		       
		       barrier2.wait();
		       std::cout << "PS thread waiting for stat update" << std::endl;
		       ps.receive_counter_statistics(barrier2,"test");
		       barrier2.wait();
		       ps.waitForDisconnect();

		       barrier2.wait();		       
		       std::cout << "PS thread terminating connection" << std::endl;
		       ps.end();
		     });
		       
  std::thread out_thr([&]{
			barrier2.wait();
			try{
			  ADNetClient net_client;
			  net_client.connect_ps(0, 0, sname);
			  barrier2.wait();
			  std::cout << "AD thread updating local stats" << std::endl;
			  ADLocalCounterStatisticsWrapper::updateGlobalStatisticsTest(net_client, "test",0,0,sname);
			  barrier2.wait();			  
			  std::cout << "AD thread terminating connection" << std::endl;
			  net_client.disconnect_ps();
			  std::cout << "AD thread waiting at barrier" << std::endl;
			  barrier2.wait();			  
			}catch(const std::exception &e){
			  std::cerr << e.what() << std::endl;
			}
		      });
  ps_thr.join();
  out_thr.join();
  
#else
#error "Requires compiling with MPI or ZMQ net"
#endif
}

TEST(ADLocalCounterStatisticsTest, UpdateGlobalStatisticsWithRealPS){  
#ifdef _USE_MPINET
#warning "Testing with MPINET not available"
#elif defined(_USE_ZMQNET)
  std::cout << "Using ZMQ net" << std::endl;

  std::string nm = "MyCounter1";
  std::unordered_set<std::string> which_counters = {nm};

  unsigned long pid = 33;
  ADLocalCounterStatistics cs(pid, 0, &which_counters);

  RunStats a,b, sum;
  for(int i=0;i<100;i++){ a.push(double(i)); b.push(double(i)*3./2); }
  sum = a+b;

  GlobalCounterStats glob;

  Barrier barrier2(2);

  std::string sinterface = "tcp://*:5559";
  std::string sname = "tcp://localhost:5559";

  int argc; char** argv = nullptr;
  std::cout << "Initializing PS thread" << std::endl;
  std::thread ps_thr([&]{
  		       ZMQNet ps;
		       ps.add_payload(new NetPayloadUpdateCounterStats(&glob));
  		       ps.init(&argc, &argv, 4); //4 workers
  		       ps.run(".");
		       std::cout << "PS thread waiting at barrier" << std::endl;
		       barrier2.wait();
		       std::cout << "PS thread terminating connection" << std::endl;
		       ps.finalize();
  		     });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::cout << "Initializing AD thread" << std::endl;
  std::thread out_thr([&]{
			try{
			  ADNetClient net_client;
			  net_client.connect_ps(0, 0, sname);

			  cs.setStats(nm, a);
			  cs.updateGlobalStatistics(net_client, 0, sname);
			  cs.setStats(nm, b);
			  cs.updateGlobalStatistics(net_client, 0, sname);

			  std::cout << "AD thread terminating connection" << std::endl;
			  net_client.disconnect_ps();
			  std::cout << "AD thread waiting at barrier" << std::endl;
			  barrier2.wait();
			}catch(const std::exception &e){
			  std::cerr << e.what() << std::endl;
			}
			//barrier2.wait();
		      });
  
  ps_thr.join();
  out_thr.join();
  
  auto gstats = glob.get_stats();
  auto pit = gstats.find(pid);
  EXPECT_NE( pit, gstats.end() );
  EXPECT_EQ( pit->second.count(nm), 1 );
  EXPECT_EQ( pit->second[nm], sum );
		         
#else
#error "Requires compiling with MPI or ZMQ net"
#endif
}




