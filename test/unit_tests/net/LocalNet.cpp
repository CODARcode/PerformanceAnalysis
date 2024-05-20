#include "gtest/gtest.h"
#include "../unit_test_common.hpp"
#include <chimbuko/core/net/local_net.hpp>

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
  int kind() const override{ return BuiltinMessageKind::CMD; }
  MessageType type() const override{ return MessageType::REQ_ECHO; }
  void action(Message &response, const Message &message) override{
    check(message);
    std::cout << "Bounce received: '" << message.getContent() << "'" << std::endl;
    response.setContent(message.getContent());
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
  msg.set_info(0,0, MessageType::REQ_ECHO, BuiltinMessageKind::CMD);
  msg.setContent(the_message);

  std::string str_reply = LocalNet::send_and_receive(msg.serializeMessage());
  Message reply;
  reply.deserializeMessage(str_reply);

  EXPECT_EQ(reply.getContent(), the_message);
}




