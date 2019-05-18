#pragma once
#ifdef _USE_ZMQNET

#include "chimbuko/net.hpp"
#include <zmq.h>
#include <boost/thread.hpp>

namespace chimbuko {

class ZMQNet : public NetInterface {
public:
    ZMQNet();
    ~ZMQNet();

    void init(int* argc, char*** argv, int nt) override;
    void finalize() override;
    void run() override;
    void stop() override;
    std::string name() const override { return "ZMQNET"; }

    void* get_context() { return m_context; }
    void* get_frontend() { return m_frontend; }
    void* get_backend() { return m_backend; }

protected:
    void init_thread_pool(int nt) override;

private:
    bool recvAndSend(void* skFrom, void* skTo);

private:
    void* m_context;
    void* m_frontend;
    void* m_backend;

    boost::thread_group m_threads;
};

} // end of chimbuko namespace
#endif // CHIMBUKO_USE_ZMQNET

/*
const int MSG_SIZE = 64;
size_t sockOptSize = sizeof(int); // 1
 
bool receiveAndSend(void* skFrom, void* skTo)
{
    int more;
    do {
        int message[MSG_SIZE];
        int len = zmq_recv(skFrom, message, MSG_SIZE, 0);
        zmq_getsockopt(skFrom, ZMQ_RCVMORE, &more, &sockOptSize);
 
        if(more == 0 && len == 0)
        {
            dump("Terminator!");
            return true;
        }
        zmq_send(skTo, message, len, more ? ZMQ_SNDMORE : 0);
    } while(more);
 
    return false;
}


void doWork(void* context) // 1
{
    void* socket = zmq_socket(context, ZMQ_REP); // 2
    zmq_connect(socket, "inproc://workers");
 
    zmq_pollitem_t items[1] = { { socket, 0, ZMQ_POLLIN, 0 } }; // 3
 
    while(true)
    {
        if(zmq_poll(items, 1, -1) < 1) // 4
        {
            dump("Terminating worker");
            break;
        }
 
        int buffer;
        int size = zmq_recv(socket, &buffer, sizeof(int), 0); // 5
        if(size < 1 || size > sizeof(int))
        {
            dump("Unexpected termination!");
            break;
        }
 
        dump("Received", buffer);
        zmq_send(socket, &buffer, size, 0);
 
        boost::this_thread::sleep(boost::posix_time::seconds(buffer));
    }
    zmq_close(socket);
}

void mtServer(int nt)
{
    boost::thread_group threads; // 1
 
    void* context = zmq_init(1);
    void* frontend = zmq_socket(context, ZMQ_ROUTER); // 2
    zmq_bind(frontend, "tcp://star:5559");
 
    void* backend = zmq_socket(context, ZMQ_DEALER); // 3
    zmq_bind(backend, "inproc://workers"); // 4
 
    for(int i = 0; i < nt; ++i)
        threads.create_thread(std::bind(&doWork, context)); // 5
 
    const int NR_ITEMS = 2; // 6
    zmq_pollitem_t items[NR_ITEMS] = 
    {
        { frontend, 0, ZMQ_POLLIN, 0 },
        { backend, 0, ZMQ_POLLIN, 0 }
    };
 
    dump("Server is ready");
    while(true)
    {
        zmq_poll(items, NR_ITEMS, -1); // 7
 
        if(items[0].revents & ZMQ_POLLIN) // 8
            if(receiveAndSend(frontend, backend))
                break;
        if(items[1].revents & ZMQ_POLLIN) // 9
            receiveAndSend(backend, frontend);
    }
 
    dump("Shutting down");
    zmq_close(frontend);
    zmq_close(backend);
    zmq_term(context);
 
    threads.join_all(); // 10
}

*/