#include<chimbuko_config.h>
#ifdef _USE_ZMQNET

#include "gtest/gtest.h"
#include "../unit_test_common.hpp"
#include <chimbuko/core/util/barrier.hpp>
#include <chimbuko/core/util/string.hpp>
#include <chimbuko/core/net/zmq_net.hpp>
#include <chimbuko/ad/ADNetClient.hpp>

using namespace chimbuko;

TEST(TestZMQNet, TimeOutWorks){
  int argc; char** argv = nullptr;
    
  ZMQNet net;
  net.init(&argc, &argv, 1);
  
  net.setTimeOut(1000);
  net.run();

  EXPECT_EQ(net.getStatus(), ZMQNet::Status::StoppedByTimeOut);

  net.finalize();
}

TEST(TestZMQNet, StopsWhenAsked){
  int argc; char** argv = nullptr;
    
  ZMQNet net;
  net.init(&argc, &argv, 1);
  
  //Give it a non-infinite timeout
  net.setTimeOut(10000); //ms
  
  std::thread stopper([&net]{
    while(net.getStatus() != ZMQNet::Status::Running){
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "Stopper thread starting countdown, T-2" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Stopper thread stopping network" << std::endl;
    net.stop();
    });

  net.run();

  EXPECT_EQ(net.getStatus(), ZMQNet::Status::StoppedByRequest);

  stopper.join();
  net.finalize();
}

class NetPayloadPing2: public NetPayloadBase{
public:
  MessageKind kind() const override{ return MessageKind::CMD; }
  MessageType type() const override{ return MessageType::REQ_GET; }
  void action(Message &response, const Message &message) override{
    check(message);
    response.set_msg(std::string("pong2"), false);
  };
};


TEST(TestZMQNet, ErrorIfMissingPayloads){

  int nt = 2;
  //Check for error if not all threads have a work item
  {
    ZMQNet net;
    net.add_payload(new NetPayloadPing, 0);
    bool err = false;
    try{
      net.init(nullptr,nullptr,nt);
    }catch(const std::exception &e){
      std::cout << "Caught expected error: " << e.what() << std::endl;
      err = true;
    }
    ASSERT_EQ(err,true);
  }
  //Check that the check that all threads have payloads mapped to the same type/kind works
  {
    ZMQNet net;
    net.add_payload(new NetPayloadPing, 0);
    net.add_payload(new NetPayloadPing, 1);
    net.add_payload(new NetPayloadPing2, 1);
    bool err = false;
    try{
      net.init(nullptr,nullptr,nt);
    }catch(const std::exception &e){
      std::cout << "Caught expected error: " << e.what() << std::endl;
      err = true;
    }
    ASSERT_EQ(err,true);
  }
}

#endif
