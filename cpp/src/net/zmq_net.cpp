#ifdef _USE_ZMQNET
#include "chimbuko/net/zmq_net.hpp"
#include "chimbuko/message.hpp"
#include "chimbuko/param/sstd_param.hpp"
#include <iostream>
#include <chrono>
#include <zmq.h>

using namespace chimbuko;

ZMQNet::ZMQNet() : m_context(nullptr), m_frontend(nullptr), m_backend(nullptr)
{
}

ZMQNet::~ZMQNet()
{
}

void ZMQNet::init(int* argc, char*** argv, int nt)
{
    void* context = zmq_init(1);
    void* frontend = zmq_socket(context, ZMQ_ROUTER); 
    void* backend = zmq_socket(context, ZMQ_DEALER); 
    
    zmq_bind(frontend, "tcp://*:5559");
    zmq_bind(backend, "inproc://workers"); 
}

void ZMQNet::finalize()
{
    if (m_frontend) zmq_close(m_frontend);
    if (m_backend) zmq_close(m_backend);
    if (m_context) zmq_term(m_context);
    m_frontend = nullptr;
    m_backend = nullptr;
    m_context = nullptr;
}

void ZMQNet::run()
{
    const int NR_ITEMS = 2; 
    zmq_pollitem_t items[NR_ITEMS] = 
    {
        { frontend, 0, ZMQ_POLLIN, 0 },
        { backend, 0, ZMQ_POLLIN, 0 }
    };
/* 
    while(true)
    {
        zmq_poll(items, NR_ITEMS, -1); 
 
        if(items[0].revents & ZMQ_POLLIN) // 8
            if(receiveAndSend(frontend, backend))
                break;

        if(items[1].revents & ZMQ_POLLIN) // 9
            receiveAndSend(backend, frontend);
    }
*/
}

void ZMQNet::stop()
{
}


#endif


