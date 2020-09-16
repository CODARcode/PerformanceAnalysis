#include<chimbuko/ad/ADNetClient.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <random>

using namespace chimbuko;

TEST(ADNetClientTestConnectPS, ConnectsMock){
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
		       ps.waitForDisconnect();
		       std::cout << "PS thread waiting at barrier" << std::endl;
		       barrier2.wait();
		       std::cout << "PS thread terminating connection" << std::endl;
		       ps.end();
		     });
		       
  std::thread out_thr([&]{
			barrier2.wait();
			try{
			  ADNetClient net_client;
			  net_client.connect_ps(0, 0, sname);
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



TEST(ADNetClientTestConnectPS, ConnectsZMQnet){  
#ifdef _USE_MPINET
#warning "Testing with MPINET not available"
#elif defined(_USE_ZMQNET)
  std::cout << "Using ZMQ net" << std::endl;

  Barrier barrier2(2);

  std::string sinterface = "tcp://*:5559";
  std::string sname = "tcp://localhost:5559";

  int argc; char** argv = nullptr;
  std::cout << "Initializing PS thread" << std::endl;
  std::thread ps_thr([&]{
  		       ZMQNet ps;
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
		         
#else
#error "Requires compiling with MPI or ZMQ net"
#endif

}
