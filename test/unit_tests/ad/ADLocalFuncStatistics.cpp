#include<chimbuko/ad/ADLocalFuncStatistics.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <random>

using namespace chimbuko;

struct ADLocalFuncStatisticsTest: public ADLocalFuncStatistics{
  static std::pair<size_t, size_t> updateGlobalStatisticsTest(ADNetClient &net_client, const std::string &l_stats, int step){
    return ADLocalFuncStatistics::updateGlobalStatistics(net_client, l_stats, step);
  }
};


TEST(ADLocalFuncStatisticsTestUpdateGlobaltatisticsWithPS, Works){
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
		       ps.receive_statistics(barrier2,"test");
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
			  ADLocalFuncStatisticsTest::updateGlobalStatisticsTest(net_client, "test",0);
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
