#include "chimbuko/net.hpp"
#include "chimbuko/net/mpi_net.hpp"
#include "chimbuko/net/zmq_net.hpp"
#include "chimbuko/param/sstd_param.hpp"

#include <iostream>

using namespace chimbuko;

NetInterface& DefaultNetInterface::get()
{
#ifdef CHIMBUKO_USE_MPINET
    static MPINet net;
    return net;
#else
    static ZMQNet net;
    return net;
#endif
}


NetInterface::NetInterface() : m_nt(0), m_tpool(nullptr)
{
}

NetInterface::~NetInterface()
{
    if (m_tpool)
    {
        delete m_tpool;
    }
}

void NetInterface::init_parameter(ParamKind kind)
{
    m_kind = kind;
    if (m_kind == ParamKind::SSTD)
    {
        m_param = new SstdParam();
    }
    else
    {
        std::cerr << "Unknown parameter kind!\n";
        exit(1);
    }
    
}

void NetInterface::init_thread_pool(int nt)
{
    if (m_tpool == nullptr && nt > 0)
    {
        m_nt = nt;
        m_tpool = new threadPool(nt);
    }
}