#include "chimbuko/net.hpp"
#include "chimbuko/net/mpi_net.hpp"
#include "chimbuko/net/zmq_net.hpp"

#include <iostream>

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

NetInterface::NetInterface() : m_nt(0), m_tpool(nullptr), m_param(nullptr), m_stop(false)
{
}

NetInterface::~NetInterface()
{
    if (m_tpool)
    {
        delete m_tpool;
    }
}

void NetInterface::set_parameter(ParamInterface* param)
{
    //m_kind = kind;
    m_param = param;
}

void NetInterface::init_thread_pool(int nt)
{
    if (m_tpool == nullptr && nt > 0)
    {
        m_nt = nt;
        m_tpool = new threadPool(nt);
    }
}
