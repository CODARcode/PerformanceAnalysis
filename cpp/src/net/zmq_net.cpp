#ifdef _USE_ZMQNET
#include "chimbuko/net/zmq_net.hpp"
#include "chimbuko/message.hpp"
#include "chimbuko/param/sstd_param.hpp"
#include <iostream>
#include <chrono>
#include <thread>

using namespace chimbuko;

ZMQNet::ZMQNet() 
: m_context(nullptr), m_frontend(nullptr), m_backend(nullptr)
{
}

ZMQNet::~ZMQNet()
{
}

void ZMQNet::init(int* /*argc*/, char*** /*argv*/, int nt)
{
    std::cout << "Init ZMQNet\n";
    m_context = zmq_ctx_new();
    m_frontend = zmq_socket(m_context, ZMQ_ROUTER);
    m_backend = zmq_socket(m_context, ZMQ_DEALER);

    zmq_bind(m_frontend, "tcp://*:5559");

    
    int rc = zmq_bind(m_backend, "inproc://workers");
    std::cout << "zmq_bind backend: " << rc << std::endl;

    // set thread pool
    std::cout << "Init. thread pool ... \n";
    init_thread_pool(nt);
}

void doWork(void* context) 
{
    int rc;
    void* socket = zmq_socket(context, ZMQ_REP); 
    rc = zmq_connect(socket, "inproc://workers");
    std::cout << "[" << std::this_thread::get_id() << "]" 
        << "zmq_connect: " << rc << std::endl;
    
    zmq_pollitem_t items[1] = { { socket, 0, ZMQ_POLLIN, 0 } }; 
 
    std::cout << "I am " << std::this_thread::get_id() << std::endl;
    while(true)
    {
        // if(zmq_poll(items, 1, -1) < 1) // 4
        // {
        //     // terminate condition
        //     std::cout << "error???\n";
        //     break;
        // }
        rc = zmq_poll(items, 1, -1);
        std::cout << "[" << std::this_thread::get_id() << "]" 
            << "zmq_poll: " << rc << std::endl;
 
        zmq_msg_t msg;
        rc = zmq_msg_init(&msg);
        std::cout << "[" << std::this_thread::get_id() << "]" 
            << "zmq_msg_init: " << rc << std::endl;
        
        int len = zmq_msg_recv(&msg, socket, 0);
        printf("Received: %d, %s\n", len, (char*)zmq_msg_data(&msg));
        zmq_msg_close(&msg);
        std::cout << "[" << std::this_thread::get_id() << "]" 
            << "done. " << std::endl;        
    }
    zmq_close(socket);
}

void ZMQNet::init_thread_pool(int nt)
{
    for(int i = 0; i < nt; ++i)
        m_threads.create_thread(std::bind(&doWork, std::ref(m_context))); 
}



void ZMQNet::finalize()
{
    if (m_frontend) zmq_close(m_frontend);
    if (m_backend) zmq_close(m_backend);
    if (m_context) zmq_ctx_term(m_context);
    m_frontend = nullptr;
    m_backend = nullptr;
    m_context = nullptr;
}

void ZMQNet::run()
{
    const int NR_ITEMS = 2; 
    zmq_pollitem_t items[NR_ITEMS] = 
    {
        { m_frontend, 0, ZMQ_POLLIN, 0 },
        { m_backend , 0, ZMQ_POLLIN, 0 }
    };


    while(true)
    {
        std::cout << "Waiting ....\n";
        zmq_poll(items, NR_ITEMS, -1); 
 
        if(items[0].revents & ZMQ_POLLIN) 
        {
            std::cout << "call recvAndSend\n";
            recvAndSend(m_frontend, m_backend);
        } 

            // if(receiveAndSend(frontend, backend))
            //     break;

        if(items[1].revents & ZMQ_POLLIN) {
            std::cout << "shouldn't happen\n";
            // retrieve request from workder and send to client

            //receiveAndSend(backend, frontend);
        }
    }
}

bool ZMQNet::recvAndSend(void* skFrom, void* skTo)
{
    std::cout << "in recvAndSend\n";
    int more, len=0, temp;
    size_t more_size = sizeof(int);

    zmq_msg_t msg;
    zmq_msg_init(&msg);

    do {
        temp = zmq_msg_recv(&msg, skFrom, 0);
        zmq_getsockopt(skFrom, ZMQ_RCVMORE, &more, &more_size);
        printf("[while] %d, %d, %s\n",
            temp, more, (char*)zmq_msg_data(&msg)
        );
        // if (more == 0 && len == 0)
        // {
        //     return true;            
        // }
        len += temp;
    } while (more);
    printf("[end] %d, %d, %s\n",
        len, more, (char*)zmq_msg_data(&msg)
    );

    temp = zmq_msg_send(&msg, skTo, 0);
    std::cout << "send to worker err code: " << temp << std::endl;
    zmq_msg_close(&msg);

    return false;
}

void ZMQNet::stop()
{
}


#endif


