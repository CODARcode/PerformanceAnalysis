#ifdef _USE_ZMQNET

#include "gtest/gtest.h"
#include "../unit_test_common.hpp"
#include <chimbuko/util/barrier.hpp>
#include <chimbuko/net/zmqme_net.hpp>
#include <chimbuko/ad/ADNetClient.hpp>

using namespace chimbuko;

//Test handshake
TEST(TestZMQMENet, CanConnect){  
  int argc; char** argv = nullptr;
    
  Barrier barrier3(3);
  
  std::string snames[2] = { "tcp://localhost:5559",
			    "tcp://localhost:5560" };

  std::thread server([&]{
      ZMQMENet net;
      net.init(&argc, &argv, 2);
      net.run(".");
      std::cout << "Net thread waiting at barrier" << std::endl;
      barrier3.wait();
      std::cout << "Net thread terminating connection" << std::endl;
      net.finalize();
    });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::vector<std::thread> clients(2);
  for(int c=0;c<2;c++){
    std::cout << "Creating AD thread " << c << std::endl;
    clients[c] = std::thread([&,c]{
	try{
	  ADNetClient net_client;
	  net_client.connect_ps(0, 0, snames[c]);
	  std::cout << "AD thread " << c << " connected" << std::endl;
	  std::this_thread::sleep_for(std::chrono::milliseconds(200));
	  std::cout << "AD thread " << c << " terminating connection" << std::endl;
	  net_client.disconnect_ps();
	  std::cout << "AD thread " << c << " waiting at barrier" << std::endl;
	  barrier3.wait();
	}catch(const std::exception &e){
	  std::cerr << "AD thread " << c << " caught an error: " << e.what() << std::endl;
	  barrier3.wait();
	}
	std::cout << "AD thread " << c << " has finished" << std::endl;
      });
  }
  
  server.join();
  for(int c=0;c<2;c++)
    clients[c].join();
}


TEST(TestZMQMENet, CanConnectMultiClientsPerEndpoint){  
  int argc; char** argv = nullptr;
    
  Barrier barrier3(3);
  
  std::string sname =  "tcp://localhost:5559" ;
		
  std::thread server([&]{
      ZMQMENet net;
      net.init(&argc, &argv, 1);
      net.run(".");
      std::cout << "Net thread waiting at barrier" << std::endl;
      barrier3.wait();
      std::cout << "Net thread terminating connection" << std::endl;
      net.finalize();
    });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::vector<std::thread> clients(2);
  for(int c=0;c<2;c++){
    std::cout << "Creating AD thread " << c << std::endl;
    clients[c] = std::thread([&,c]{
	try{
	  ADNetClient net_client;
	  net_client.connect_ps(0, 0, sname);
	  std::cout << "AD thread " << c << " connected" << std::endl;
	  std::this_thread::sleep_for(std::chrono::milliseconds(200));
	  std::cout << "AD thread " << c << " terminating connection" << std::endl;
	  net_client.disconnect_ps();
	  std::cout << "AD thread " << c << " waiting at barrier" << std::endl;
	  barrier3.wait();
	}catch(const std::exception &e){
	  std::cerr << "AD thread " << c << " caught an error: " << e.what() << std::endl;
	  barrier3.wait();
	}
	std::cout << "AD thread " << c << " has finished" << std::endl;
      });
  }
  
  server.join();
  for(int c=0;c<2;c++)
    clients[c].join();
}



class NetPayloadIncrement: public NetPayloadBase{
public:
  int &i;
  std::mutex m;

  NetPayloadIncrement(int &i): i(i){}

  MessageKind kind() const{ return MessageKind::CMD; }
  MessageType type() const{ return MessageType::REQ_ADD; }
  void action(Message &response, const Message &message) override{
    check(message);
    std::lock_guard<std::mutex> _(m);
    i++;
    response.set_msg("", false);
  };
};


//Test payload
TEST(TestZMQMENet, PayloadOperationsWork){  
  int argc; char** argv = nullptr;
    
  Barrier barrier3(3);
  
  std::string snames[2] = { "tcp://localhost:5559",
			    "tcp://localhost:5560" };

  int i = 0;

  std::thread server([&]{
      ZMQMENet net;
      net.init(&argc, &argv, 2);
      net.add_payload(new NetPayloadIncrement(i), 0);
      net.add_payload(new NetPayloadIncrement(i), 1);

      net.run(".");
      std::cout << "Net thread waiting at barrier" << std::endl;
      barrier3.wait();
      std::cout << "Net thread terminating connection" << std::endl;
      net.finalize();
    });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::vector<std::thread> clients(2);
  for(int c=0;c<2;c++){
    std::cout << "Creating AD thread " << c << std::endl;
    clients[c] = std::thread([&,c]{
	try{
	  ADNetClient net_client;
	  net_client.connect_ps(0, 0, snames[c]);
	  std::cout << "AD thread " << c << " connected" << std::endl;
	  std::this_thread::sleep_for(std::chrono::milliseconds(200));

	  std::cout << "AD thread " << c << " sending message" << std::endl;
	  Message msg;
	  msg.set_info(0,0, MessageType::REQ_ADD, MessageKind::CMD);
	  msg.set_msg("");
	  net_client.send_and_receive(msg);

	  std::this_thread::sleep_for(std::chrono::milliseconds(200));
	  std::cout << "AD thread " << c << " terminating connection" << std::endl;
	  net_client.disconnect_ps();
	  std::cout << "AD thread " << c << " waiting at barrier" << std::endl;
	  barrier3.wait();
	}catch(const std::exception &e){
	  std::cerr << "AD thread " << c << " caught an error: " << e.what() << std::endl;
	  barrier3.wait();
	}
	std::cout << "AD thread " << c << " has finished" << std::endl;
      });
  }
  
  server.join();
  for(int c=0;c<2;c++)
    clients[c].join();

  EXPECT_EQ(i,2);
}



#endif
