#ifdef _USE_ZMQNET
#include "chimbuko/net/zmq_net.hpp"
#include "chimbuko/message.hpp"
#include "chimbuko/param/sstd_param.hpp"
#include <iostream>
#include <string.h>

using namespace chimbuko;

ZMQNet::ZMQNet() : m_context(nullptr), m_n_requests(0)
{
}

ZMQNet::~ZMQNet()
{
}

void ZMQNet::init(int* /*argc*/, char*** /*argv*/, int nt)
{
    m_context  = zmq_ctx_new();
    init_thread_pool(nt);
}

void doWork(void* context, ParamInterface* param) 
{
    void* socket = zmq_socket(context, ZMQ_REP); 
    zmq_connect(socket, "inproc://workers");
    
    zmq_pollitem_t items[1] = { { socket, 0, ZMQ_POLLIN, 0 } }; 
 
    while(true)
    {
        if(zmq_poll(items, 1, -1) < 1) 
        {
            break;
        }
 
        std::string strmsg;
        ZMQNet::recv(socket, strmsg);

        // -------------------------------------------------------------------
        // this block could be managed to use for all network interface!!
        // todo: will move this part somewhere to make the code simple.
        Message msg, msg_reply;
        msg.set_msg(strmsg, true);

        msg_reply = msg.createReply();
        if (msg.kind() == MessageKind::SSTD)
        {
            SstdParam* p = (SstdParam*)param;
            if (msg.type() == MessageType::REQ_ADD) {
                //std::cout << "REQ_ADD" << std::endl;
                msg_reply.set_msg(p->update(msg.data_buffer(), true), false);
            }
            else if (msg.type() == MessageType::REQ_GET) {
                //std::cout << "REQ_GET" << std::endl;
                msg_reply.set_msg(p->serialize(), false);
            }
        }
        else if (msg.kind() == MessageKind::DEFAULT)
        {
            if (msg.type() == MessageType::REQ_ECHO) {
                //std::cout << "REQ_ECHO" << std::endl;
                msg_reply.set_msg(msg.data_buffer() + ">I am ZMQNET!", false);
            }
        }
        else 
        {
            std::cout << "Unknow" << std::endl;
        }
        // -------------------------------------------------------------------

        ZMQNet::send(socket, msg_reply.data());
    }
    zmq_close(socket);
}

void ZMQNet::init_thread_pool(int nt)
{
    for (int i = 0; i < nt; i++) {
        m_threads.push_back(
            std::thread(&doWork, std::ref(m_context), std::ref(m_param))    
        );
    }
}

void ZMQNet::finalize()
{
    if (m_context) zmq_ctx_term(m_context);
    m_context = nullptr;
    for (auto& t: m_threads)
        if (t.joinable())
            t.join();
}

void ZMQNet::run()
{    
    void* frontend = zmq_socket(m_context, ZMQ_ROUTER);
    void* backend  = zmq_socket(m_context, ZMQ_DEALER);

    zmq_bind(frontend, "tcp://*:5559");
    zmq_bind(backend, "inproc://workers");

    const int NR_ITEMS = 2; 
    zmq_pollitem_t items[NR_ITEMS] = 
    {
        { frontend, 0, ZMQ_POLLIN, 0 },
        { backend , 0, ZMQ_POLLIN, 0 }
    };

    m_n_requests = 0;
    while(true)
    {
        zmq_poll(items, NR_ITEMS, -1); 
 
        if(items[0].revents & ZMQ_POLLIN) { 
            if (recvAndSend(frontend, backend)) {
                stop();
                break;
            }
            m_n_requests++;
        }

        if(items[1].revents & ZMQ_POLLIN) {
            recvAndSend(backend, frontend);
            m_n_requests--;
        } 
    }

    zmq_close(frontend);
    zmq_close(backend);
}

bool ZMQNet::recvAndSend(void* skFrom, void* skTo)
{
    int more, len;
    size_t more_size = sizeof(int);

    do {
        zmq_msg_t msg;
        zmq_msg_init(&msg);

        len = zmq_msg_recv(&msg, skFrom, 0);
        zmq_getsockopt(skFrom, ZMQ_RCVMORE, &more, &more_size);
        if (more == 0 && len == 0)
        {
            return true;
        }
        zmq_msg_send(&msg, skTo, more ? ZMQ_SNDMORE: 0);
        zmq_msg_close(&msg);
    } while (more);

    return false;
}

void ZMQNet::stop()
{
    int n_tries = 60;
    while (m_n_requests && n_tries) {
        n_tries--;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int ZMQNet::send(void* socket, const std::string& strmsg)
{
    zmq_msg_t msg;
    int ret;
    // zero-copy version (need to think again...)
    // zmq_msg_init_data(
    //     &msg, (void*)strmsg.data(), strmsg.size(), 
    //     [](void* _data, void* _hint){
    //         // std::cout << "free message\n";
    //         // free(_data);
    //     }, 
    //     nullptr
    // );
    // internal-copy version
    zmq_msg_init_size(&msg, strmsg.size());
    memcpy(zmq_msg_data(&msg), (const void*)strmsg.data(), strmsg.size());
    ret = zmq_msg_send(&msg, socket, 0);
    zmq_msg_close(&msg);
    return ret;
}

int ZMQNet::recv(void* socket, std::string& strmsg)
{
    zmq_msg_t msg;
    int ret;
    zmq_msg_init(&msg);
    ret = zmq_msg_recv(&msg, socket, 0);
    strmsg.assign((char*)zmq_msg_data(&msg), ret);
    zmq_msg_close(&msg);
    return ret;
}


#endif


