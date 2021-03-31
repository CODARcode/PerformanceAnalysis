#include<chimbuko/ad/ADNetClient.hpp>
#include<chimbuko/util/string.hpp>
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


TEST(ADNetClientTestConnectPS, ConnectionTimeoutWorks){  
#ifdef _USE_MPINET
#warning "Testing with MPINET not available"
#elif defined(_USE_ZMQNET)
  std::string sname = "tcp://localhost:55599"; //this should be on a different port than other tests as it is apparently left floating in the ether

  ADNetClient net_client;
  net_client.setRecvTimeout(100); //100ms
  net_client.connect_ps(0, 0, sname);
  
  EXPECT_EQ( net_client.use_ps(), false );
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



class NetPayloadBounceback: public NetPayloadBase{
public:
  MessageKind kind() const{ return MessageKind::CMD; }
  MessageType type() const{ return MessageType::REQ_ECHO; }
  void action(Message &response, const Message &message) override{
    check(message);
    std::cout << "Bounce received: '" << message.buf() << "'" << std::endl;
    response.set_msg(message.buf(), false);
  };
};

TEST(ADNetClientTestConnectPS, SendRecvZMQnet){  
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
		       ps.add_payload(new NetPayloadBounceback);
  		       ps.init(&argc, &argv, 4); //4 workers
  		       ps.run(".");
		       std::cout << "PS thread waiting at barrier" << std::endl;
		       barrier2.wait();
		       std::cout << "PS thread terminating connection" << std::endl;
		       ps.finalize();
  		     });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  bool success = true;
  std::string response;

  std::cout << "Initializing AD thread" << std::endl;
  std::thread out_thr([&]{
			try{
			  ADNetClient net_client;
			  net_client.connect_ps(0, 0, sname);

			  Message msg;
			  msg.set_info(0,0,REQ_ECHO,CMD);
			  msg.set_msg("Hello!!");

			  try{
			    net_client.send_and_receive(msg, msg);
			  }catch(std::exception &e){
			    std::cout << "Got unexpected error: " << e.what();
			    success = false;
			  }
			  if(success) response = msg.buf();

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
  
  EXPECT_EQ(success, true);

  std::cout << "Received response: '" << response << "'" << std::endl;
  EXPECT_EQ(response, "Hello!!");

#else
#error "Requires compiling with MPI or ZMQ net"
#endif

}


class NetPayloadBouncebackDelayed: public NetPayloadBase{
public:
  MessageKind kind() const{ return MessageKind::CMD; }
  MessageType type() const{ return MessageType::REQ_ECHO; }
  void action(Message &response, const Message &message) override{
    check(message);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    response.set_msg(message.buf(), false);
  };
};


TEST(ADNetClientTestConnectPS, TestSendRecvTimeoutZMQnet){  
#ifdef _USE_MPINET
#warning "Testing with MPINET not available"
#elif defined(_USE_ZMQNET)

  Barrier barrier2(2);

  int port = 555911;
  std::string sname = "tcp://localhost:" + anyToStr(port);

  int argc; char** argv = nullptr;
  std::cout << "Initializing PS thread" << std::endl;
  std::thread ps_thr([&]{
  		       ZMQNet ps;
		       ps.add_payload(new NetPayloadBouncebackDelayed);
  		       ps.init(&argc, &argv, 4); //4 workers
		       ps.setPort(port);
  		       ps.run(".");
		       std::cout << "PS thread waiting at barrier" << std::endl;
		       barrier2.wait();
		       std::cout << "PS thread terminating connection" << std::endl;
		       ps.finalize();
  		     });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  bool success = true;

  std::cout << "Initializing AD thread" << std::endl;
  std::thread out_thr([&]{
			try{
			  ADNetClient net_client;
			  net_client.setRecvTimeout(1000);
			  net_client.connect_ps(0, 0, sname);

			  Message msg;
			  msg.set_info(0,0,REQ_ECHO,CMD);
			  msg.set_msg("Hello!!");

			  try{
			    net_client.send_and_receive(msg, msg);
			  }catch(std::exception &e){
			    std::cout << "Got expected error: " << e.what();
			    success = false;
			  }

			  //We need to actually receive the message to allow the client to disconnect cleanly
			  {
			    zmq_msg_t msg;
			    zmq_msg_init(&msg);
			    zmq_msg_recv(&msg, net_client.getZMQsocket(), 0);    
			    zmq_msg_close(&msg);
			  }

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
  
  EXPECT_EQ(success, false);

#else
#error "Requires compiling with MPI or ZMQ net"
#endif

}




