#include "chimbuko/net.hpp"
#include "chimbuko/net/mpi_net.hpp"
#include "chimbuko/net/zmq_net.hpp"

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
