#include "chimbuko/message.hpp"
#include "chimbuko/verbose.hpp"
#include "chimbuko/core/util/string.hpp"
#include "chimbuko/core/util/error.hpp"
#include "chimbuko/core/net/local_net.hpp"
#include <iostream>
#include <string.h>

using namespace chimbuko;

LocalNet * & LocalNet::globalInstance(){ static LocalNet* net = nullptr; return net; }

LocalNet::LocalNet(){
  if(globalInstance() != nullptr) fatal_error("Cannot have 2 instances of LocalNet active");
  globalInstance() = this;
}

LocalNet::~LocalNet(){
  globalInstance() = nullptr;
}

std::string LocalNet::send_and_receive(const std::string &send_str){
  if(globalInstance() == nullptr){ fatal_error("An instance of LocalNet must exist"); }

  Message msg, msg_reply;
  msg.set_msg(send_str, true);
  
  NetInterface::find_and_perform_action(0, msg_reply, msg, globalInstance()->m_payloads);
  return msg_reply.data();
}
