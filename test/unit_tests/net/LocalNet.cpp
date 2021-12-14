#include "gtest/gtest.h"
#include "../unit_test_common.hpp"
#include <chimbuko/net/local_net.hpp>

using namespace chimbuko;

TEST(TestLocalNet, TestSingleton){
  LocalNet *net1 = new LocalNet;
  bool err = false;

  try{
    LocalNet net2;
  }catch(const std::exception &e){
    err = true;
  }
  
  EXPECT_EQ(err, true);

  delete net1;

  err = false;
  try{
    LocalNet net2;
  }catch(const std::exception &e){
    err = true;
  }

  EXPECT_EQ(err, false);
}

class NetPayloadBounceback: public NetPayloadBase{
public:
  MessageKind kind() const override{ return MessageKind::CMD; }
  MessageType type() const override{ return MessageType::REQ_ECHO; }
  void action(Message &response, const Message &message) override{
    check(message);
    std::cout << "Bounce received: '" << message.buf() << "'" << std::endl;
    response.set_msg(message.buf(), false);
  };
};


TEST(TestLocalNet, SendAndReceive){
  //Test it throws if no active instance
  bool err = false;
  try{
    LocalNet::send_and_receive("hello");
  }catch(const std::exception &e){
    err = true;
  }
  EXPECT_EQ(err, true);

  err = false;

  LocalNet net;
  net.add_payload(new NetPayloadBounceback);

  std::string the_message = "Hello!";
  Message msg;
  msg.set_info(0,0, MessageType::REQ_ECHO, MessageKind::CMD);
  msg.set_msg(the_message);

  std::string str_reply = LocalNet::send_and_receive(msg.data());
  Message reply;
  reply.set_msg(str_reply, true);

  EXPECT_EQ(reply.buf(), the_message);
}




