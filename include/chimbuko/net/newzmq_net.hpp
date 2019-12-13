#pragma once
#ifdef _USE_ZMQNET

#include "chimbuko/net.hpp"
#include <zmq.h>
#include <vector>
#include <thread>
#include <queue>
#include "chimbuko/zmq_net.hpp"


namespace chimbuko {

class NZMQNet : public ZMQNet {
public:
    NZMQNet();
    ~NZMQNet();
    static void do_some_work();	

};

} // end of chimbuko namespace
#endif // CHIMBUKO_USE_ZMQNET
