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
  add_payload(new NetPayloadHandShake());
}

NetInterface::~NetInterface()
{
}

void NetInterface::add_payload(NetPayloadBase* payload){
  m_payloads[payload->kind()][payload->type()].reset(payload);
}

