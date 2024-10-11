#include<chimbuko/core/ad/ADNetClient.hpp>
#include<chimbuko/core/util/string.hpp>
#include "gtest/gtest.h"
#include "../../../unit_test_common.hpp"

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <random>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

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
			  ADZMQNetClient net_client;
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

  ADZMQNetClient net_client;
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
			  ADZMQNetClient net_client;
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
  int kind() const override{ return BuiltinMessageKind::CMD; }
  MessageType type() const override{ return MessageType::REQ_ECHO; }
  void action(Message &response, const Message &message) override{
    check(message);
    std::cout << "Bounce received: '" << message.getContent() << "'" << std::endl;
    response.setContent(message.getContent());
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
      int nt = 4;  //4 workers
      ZMQNet ps;
      for(int i=0;i<4;i++) ps.add_payload(new NetPayloadBounceback,i);
      ps.init(&argc, &argv, nt);
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
			  ADZMQNetClient net_client;
			  net_client.connect_ps(0, 0, sname);

			  Message msg;
			  msg.set_info(0,0,REQ_ECHO,CMD);
			  msg.setContent("Hello!!");

			  try{
			    net_client.send_and_receive(msg, msg);
			  }catch(std::exception &e){
			    std::cout << "Got unexpected error: " << e.what();
			    success = false;
			  }
			  if(success) response = msg.getContent();

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
  int kind() const override{ return BuiltinMessageKind::CMD; }
  MessageType type() const override{ return MessageType::REQ_ECHO; }
  void action(Message &response, const Message &message) override{
    check(message);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    response.setContent(message.getContent());
  };
};

//Induce a timeout by having the pserver delay its response for longer than the timeout
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
      int nt = 4; //4 workers
      ZMQNet ps;
      for(int i=0;i<nt;i++) ps.add_payload(new NetPayloadBouncebackDelayed,i);
      ps.init(&argc, &argv, 4); 
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
			  ADZMQNetClient net_client;
			  net_client.setRecvTimeout(1000);
			  net_client.connect_ps(0, 0, sname);

			  Message msg;
			  msg.set_info(0,0,REQ_ECHO,CMD);
			  msg.setContent("Hello!!");

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





TEST(ADNetClient, TestRemoteStop){  
#ifdef _USE_MPINET
#warning "Testing with MPINET not available"
#elif defined(_USE_ZMQNET)

  Barrier barrier2(2);

  int port = 555912;
  std::string sname = "tcp://localhost:" + anyToStr(port);

  int argc; char** argv = nullptr;
  std::cout << "Initializing PS thread" << std::endl;
  
  bool stopped_on_request = false;

  std::thread ps_thr([&]{
  		       ZMQNet ps;
  		       ps.init(&argc, &argv, 4); //4 workers
		       ps.setPort(port);
		       ps.setAutoShutdown(false);
  		       ps.run(".");
		       std::cout << "PS thread waiting at barrier" << std::endl;
		       barrier2.wait();
		       std::cout << "PS thread terminating connection" << std::endl;
		       ps.finalize();
		       stopped_on_request = ps.getStatus() == ZMQNet::Status::StoppedByRequest;
  		     });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::cout << "Initializing AD thread" << std::endl;
  std::thread out_thr([&]{
			try{
			  ADZMQNetClient net_client;
			  net_client.setRecvTimeout(1000);
			  net_client.connect_ps(0, 0, sname);

			  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			  std::cout << "AD thread issuing stop request" << std::endl;
			  net_client.stopServer();

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
  
  EXPECT_EQ(stopped_on_request, true);

#else
#error "Requires compiling with MPI or ZMQ net"
#endif

}


class NetPayloadGetValue: public NetPayloadBase{
  int val;
public:
  NetPayloadGetValue(int val): val(val){}

  int kind() const override{ return BuiltinMessageKind::CMD; }
  MessageType type() const override{ return MessageType::REQ_ECHO; }
  void action(Message &response, const Message &message) override{
    check(message);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    response.setContent(std::to_string(val));
  };
};


TEST(ADNetClientTestConnectPS, SendRecvThreadZMQnet){  
  //Test a comms pattern where each worker thread reads from a separate object
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
      int nt = 4;  //4 workers
      int vals[4] = {11,22,33,44};

      ZMQNet ps;
      for(int i=0;i<4;i++) ps.add_payload(new NetPayloadGetValue(vals[i]),i);
      ps.init(&argc, &argv, nt);
      ps.run(".");
      std::cout << "PS thread waiting at barrier" << std::endl;
      barrier2.wait();
      std::cout << "PS thread terminating connection" << std::endl;
      ps.finalize();
    });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  bool success = true;
  std::set<std::string> responses;

  std::cout << "Initializing AD thread" << std::endl;
  std::thread out_thr([&]{
      try{
	ADZMQNetClient net_client;
	net_client.connect_ps(0, 0, sname);

	for(int i=0;i<4;i++){
	  Message msg;
	  msg.set_info(0,0,REQ_ECHO,CMD);
	  msg.setContent("Hello!!");

	  try{
	    net_client.send_and_receive(msg, msg);
	  }catch(std::exception &e){
	    std::cout << "Got unexpected error: " << e.what();
	    success = false;
	  }
	  if(success) responses.insert(msg.getContent());
	  else break;
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
  
  EXPECT_EQ(success, true);
  EXPECT_EQ(responses.count("11"),1);
  EXPECT_EQ(responses.count("22"),1);
  EXPECT_EQ(responses.count("33"),1);
  EXPECT_EQ(responses.count("44"),1);

#else
#error "Requires compiling with MPI or ZMQ net"
#endif

}



TEST(ADLocalNetClientTest, SendRecv){  
  ADLocalNetClient client;
  bool err = false;
  try{
    client.connect_ps(1,2,"arg");
  }catch(const std::exception &e){
    err = true;
  }
  EXPECT_EQ(err,true);
  EXPECT_EQ(client.use_ps(),false);

  err = false;
  LocalNet net;
  net.add_payload(new NetPayloadBounceback);
  
  try{
    client.connect_ps(1,2,"arg");
  }catch(const std::exception &e){
    err = true;
  }
  
  EXPECT_EQ(err, false);
  EXPECT_EQ(client.use_ps(), true);
  
  Message msg, msg_reply;
  msg.set_info(0,0,REQ_ECHO,CMD);
  msg.setContent("Hello!!");

  client.send_and_receive(msg_reply, msg);

  EXPECT_EQ(msg_reply.getContent(), "Hello!!");

  client.disconnect_ps();
  EXPECT_EQ(client.use_ps(), false);
}

