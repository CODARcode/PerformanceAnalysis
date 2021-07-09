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

  unsigned long pid = 199, pid2 = 256;
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

  unsigned long vals[5];

  std::cout << "Initializing AD thread" << std::endl;
  std::thread out_thr([&]{
      try{
	ADThreadNetClient net_client;
	net_client.connect_ps(0, 0, sname);

	ADglobalFunctionIndexMap local_map(pid, &net_client);
	vals[0] = local_map.lookup(22, "hello");
	vals[1] = local_map.lookup(55, "world");

	vals[2] = local_map.lookup(22, ""); //second lookup doesn't need function name
	vals[3] = local_map.lookup(55, "");

	//Ensure that different program indices but the same function name are distinguished
	ADglobalFunctionIndexMap local_map2(pid2, &net_client);
	vals[4] = local_map2.lookup(22, "hello");

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
  EXPECT_EQ(vals[4], 2);

#else
#error "Requires compiling with MPI or ZMQ net"
#endif
}


TEST(ADglobalFunctionIndexMapTest, RetrieveGlobalIndexBatchedWithRealPS){
#ifdef _USE_MPINET
#warning "Testing with MPINET not available"
#elif defined(_USE_ZMQNET)
  std::cout << "Using ZMQ net" << std::endl;

  Barrier barrier2(2);

  unsigned long pid = 199, pid2 = 256;
  std::string sinterface = "tcp://*:5559";
  std::string sname = "tcp://localhost:5559";

  int argc; char** argv = nullptr;
  std::cout << "Initializing PS thread" << std::endl;
  std::thread ps_thr([&]{
      PSglobalFunctionIndexMap glob_map;
      ZMQNet ps;
      ps.add_payload(new NetPayloadGlobalFunctionIndexMapBatched(&glob_map));
      ps.init(&argc, &argv, 4); //4 workers
      ps.run(".");
      std::cout << "PS thread waiting at barrier" << std::endl;
      barrier2.wait();
      std::cout << "PS thread terminating connection" << std::endl;
      ps.finalize();
    });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::vector<unsigned long> loc_idx = { 22,33,44,55 };
  std::vector<std::string> func_names = { "hello", "world", "foo", "bar" };

  std::vector<unsigned long> vals1, vals2, vals3;

  std::cout << "Initializing AD thread" << std::endl;
  std::thread out_thr([&]{
      try{
	ADThreadNetClient net_client;
	net_client.connect_ps(0, 0, sname);

	ADglobalFunctionIndexMap local_map(pid, &net_client);
	vals1 = local_map.lookup(loc_idx, func_names);
	vals2 = local_map.lookup(loc_idx, func_names);

	//Ensure same function names but different program indices distinguished
	ADglobalFunctionIndexMap local_map2(pid2, &net_client);
	vals3 = local_map2.lookup(loc_idx, func_names);

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

  EXPECT_EQ(vals1.size(), 4);
  EXPECT_EQ(vals1[0], 0);
  EXPECT_EQ(vals1[1], 1);
  EXPECT_EQ(vals1[2], 2);
  EXPECT_EQ(vals1[3], 3);

  EXPECT_EQ(vals2.size(), 4);
  EXPECT_EQ(vals2[0], 0);
  EXPECT_EQ(vals2[1], 1);
  EXPECT_EQ(vals2[2], 2);
  EXPECT_EQ(vals2[3], 3);

  EXPECT_EQ(vals3.size(), 4);
  EXPECT_EQ(vals3[0], 4);
  EXPECT_EQ(vals3[1], 5);
  EXPECT_EQ(vals3[2], 6);
  EXPECT_EQ(vals3[3], 7);

#else
#error "Requires compiling with MPI or ZMQ net"
#endif
}



TEST(ADglobalFunctionIndexMapTest, RetrieveGlobalIndexBatchedOneFuncWithRealPS){
#ifdef _USE_MPINET
#warning "Testing with MPINET not available"
#elif defined(_USE_ZMQNET)
  std::cout << "Using ZMQ net" << std::endl;

  Barrier barrier2(2);

  unsigned long pid = 199;
  std::string sinterface = "tcp://*:5559";
  std::string sname = "tcp://localhost:5559";

  int argc; char** argv = nullptr;
  std::cout << "Initializing PS thread" << std::endl;
  std::thread ps_thr([&]{
      PSglobalFunctionIndexMap glob_map;
      ZMQNet ps;
      ps.add_payload(new NetPayloadGlobalFunctionIndexMapBatched(&glob_map));
      ps.init(&argc, &argv, 4); //4 workers
      ps.run(".");
      std::cout << "PS thread waiting at barrier" << std::endl;
      barrier2.wait();
      std::cout << "PS thread terminating connection" << std::endl;
      ps.finalize();
    });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::vector<unsigned long> loc_idx = { 22 };
  std::vector<std::string> func_names = { "hello" };

  std::vector<unsigned long> vals1, vals2;

  std::cout << "Initializing AD thread" << std::endl;
  std::thread out_thr([&]{
      try{
	ADThreadNetClient net_client;
	net_client.connect_ps(0, 0, sname);

	ADglobalFunctionIndexMap local_map(pid, &net_client);
	vals1 = local_map.lookup(loc_idx, func_names);
	vals2 = local_map.lookup(loc_idx, func_names);

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

  EXPECT_EQ(vals1.size(), 1);
  EXPECT_EQ(vals1[0], 0);

  EXPECT_EQ(vals2.size(), 1);
  EXPECT_EQ(vals2[0], 0);

#else
#error "Requires compiling with MPI or ZMQ net"
#endif
}
