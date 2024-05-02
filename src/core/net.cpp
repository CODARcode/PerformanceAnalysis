#include "chimbuko/core/net.hpp"
#include "chimbuko/core/net/mpi_net.hpp"
#include "chimbuko/core/net/zmq_net.hpp"
#include "chimbuko/core/util/error.hpp"

#include <iostream>
#include <chrono>
#include <sstream>
#include <string>
#include <nlohmann/json.hpp>

using namespace chimbuko;

NetInterface& DefaultNetInterface::get()
{
#ifdef _USE_MPINET
    static MPINet net;
    return net;
#else
    static ZMQNet net;
    return net;
#endif
}

NetInterface::NetInterface() 
  : m_nt(0)
{
}

NetInterface::~NetInterface()
{
}

void NetInterface::add_payload(NetPayloadBase* payload, int worker_idx){
  m_payloads[worker_idx][payload->kind()][payload->type()].reset(payload);
}

void NetInterface::list_payloads(std::ostream &os) const{
  for(auto const &w : m_payloads){
    for(auto const &k : w.second){      
      for(auto const &t : k.second){
	os << "worker:" << w.first << " kind:" << k.first << " type:" << t.first << std::endl;
      }
    }
  }
}

void NetInterface::find_and_perform_action(int worker_id, Message &msg_reply, const Message &msg, const NetInterface::WorkerPayloadMapType &payloads){
  auto iit = payloads.find(worker_id);
  if(iit == payloads.end()){ fatal_error(std::string("Cannot find payloads with worker index ") + std::to_string(worker_id) ); }
  find_and_perform_action(msg_reply, msg, iit->second);
}

void NetInterface::find_and_perform_action(Message &msg_reply, const Message &msg, const NetInterface::PayloadMapType &payloads){  
  auto kit = payloads.find((MessageKind)msg.kind());
  if(kit == payloads.end()) fatal_error("No payload associated with the message kind provided. Message: " + msg.buf() + " (did you add the payload to the server?)");
  auto pit = kit->second.find((MessageType)msg.type());
  if(pit == kit->second.end()) fatal_error("No payload associated with the message type provided. Mesage: " + msg.buf() + " (did you add the payload to the server?)");

  msg_reply = msg.createReply();
  pit->second->action(msg_reply, msg);
}


