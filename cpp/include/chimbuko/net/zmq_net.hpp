#pragma once
#ifdef _USE_ZMQNET

#include "chimbuko/net.hpp"

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

private:
    void* m_context;
    void* m_frontend;
    void* m_backend;
};

/*
size_t int MSG_SIZE = 64;
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





 * */


} // end of chimbuko namespace
#endif // CHIMBUKO_USE_ZMQNET
