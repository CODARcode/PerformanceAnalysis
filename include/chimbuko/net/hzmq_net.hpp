#pragma once
#ifdef _USE_ZMQNET

#include "chimbuko/net.hpp"
#include <zmq.h>
#include <vector>
#include <thread>
#include <string>
#include "chimbuko/util/mtQueue.hpp"

namespace chimbuko {

class hZMQNet : public  ZMQNet {
public:
    hZMQNet();
    ~hZMQNet();

    void set_server_name();
    void init_client_mode(std::string, mtQueue<std::string>&);
    void init_server_mode(char*);	
    static void do_some_work (std::string , mtQueue<std::string>&);
    static void start_subscription(char*);
    static void start_publisher();		
    void h_ifinalize();
    void stop_client();
    void init_sub(char*);
    void init_pub();	
    void set_parameter(ParamInterface* param);
    ParamInterface* get_parameter() { return hm_param;}	


private:
    void* hm_context;
    void* ptr_client;
    void* ptr_pub;
    void* ptr_sub; 	
    std::string name;
    std::string server;
    ParamInterface* hm_param; // pointer to parameter(storage) 	
};

} // end of chimbuko namespace
#endif // CHIMBUKO_USE_ZMQNET
