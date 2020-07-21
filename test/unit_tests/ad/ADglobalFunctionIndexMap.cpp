#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include <chimbuko/ad/ADglobalFunctionIndexMap.hpp>
#include <chimbuko/pserver/PSglobalFunctionIndexMap.hpp>

using namespace chimbuko;



TEST(ADglobalFunctionIndexMapTest, RetrieveGlobalIndexWithRealPS){  
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
      PSglobalFunctionIndexMap glob_map;
      ZMQNet ps;
      ps.add_payload(new NetPayloadGlobalFunctionIndexMap(&glob_map));
      ps.init(&argc, &argv, 4); //4 workers
      ps.run(".");
      std::cout << "PS thread waiting at barrier" << std::endl;
      barrier2.wait();
      std::cout << "PS thread terminating connection" << std::endl;
      ps.finalize();
    });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  unsigned long vals[4];

  std::cout << "Initializing AD thread" << std::endl;
  std::thread out_thr([&]{
      try{
	ADNetClient net_client;
	net_client.connect_ps(0, 0, sname);
	
	ADglobalFunctionIndexMap local_map(&net_client);
	vals[0] = local_map.lookup(22, "hello");
	vals[1] = local_map.lookup(55, "world");

	vals[2] = local_map.lookup(22, ""); //second lookup doesn't need function name
	vals[3] = local_map.lookup(55, "");
	
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

  EXPECT_EQ(vals[0], 0);
  EXPECT_EQ(vals[1], 1);
  EXPECT_EQ(vals[2], 0);
  EXPECT_EQ(vals[3], 1);
    		         
#else
#error "Requires compiling with MPI or ZMQ net"
#endif
}
