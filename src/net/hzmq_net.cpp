#ifdef _USE_ZMQNET
#include "chimbuko/net/zmq_net.hpp"
#include "chimbuko/message.hpp"
#include "chimbuko/param/sstd_param.hpp"
#include <iostream>
#include <string.h>
#include "chimbuko/net/hzmq_net.hpp"
#include <random>
//#include <chrono>
//#include <thread>
#ifdef _PERF_METRIC
#include <fstream>
#include <chrono>
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MilliSec;
#endif

using namespace chimbuko;

hZMQNet::hZMQNet() : hm_context(nullptr)
{
}

hZMQNet::~hZMQNet()
{
}

void hZMQNet::init_client_mode( std::string str, mtQueue<std::string>& qu)
{
     ptr_client=new std::thread(&do_some_work, str,  std::ref(qu));
    //hm_context  = zmq_ctx_new();
}

void hZMQNet::init_sub(char* str){
    std::cout << "Initializing the subscriber" << std::endl;
    ptr_sub= new std::thread(&start_subscription, str);	
}


void hZMQNet::init_pub(){
	std::cout << "Initializing the publisher" << std::endl;
	ptr_pub= new std::thread(&start_publisher);
}


void hZMQNet::start_publisher(){

         void* socket;
    	 void* context;
    	 context = zmq_ctx_new();
    	 socket = zmq_socket(context, ZMQ_PUB);


  	int rc = zmq_bind(socket, "tcp://*:6000");

	std::cout << "I am in the publisher[1]" << std::endl;
	std::cout << "[rc]" << rc << std::endl;
	//exit(1);	
  	//sleep(1);
  	std::this_thread::sleep_for(std::chrono::milliseconds(1));
  	char message[1000] = "Publisher is testing\n";
  	while(1) {
    		zmq_msg_t out_msg;
    		zmq_msg_init_size(&out_msg, strlen(message));
    		memcpy(zmq_msg_data(&out_msg), message, strlen(message));
    		zmq_send(socket, &out_msg,1000, 0);
    		zmq_msg_close(&out_msg);
    		//sleep(1);
    		std::this_thread::sleep_for(std::chrono::milliseconds(20));
  	}

}


void hZMQNet::start_subscription(char* str){
// start the subsription mode
// We need to know the address of the publisher to get the message
//
//


	printf("%s\n", str);

	void *context = zmq_ctx_new();
	void *subscriber = zmq_socket(context, ZMQ_SUB);
 	int rc = zmq_connect ( subscriber, str);
	zmq_setsockopt ( subscriber, ZMQ_SUBSCRIBE, "", 0);
        //std::cout << str << std::endl;
	std::cout << "I am in the subsciber[2]" << std::endl;
        std::cout << "[sub-rc]" << rc << std::endl;
       // exit(1);
	
	char string[1000] = "";
	while (true){
		zmq_msg_t in_msg;
		zmq_msg_init(&in_msg);
		zmq_recv(subscriber, &in_msg,1000, 0);
		int size = zmq_msg_size(&in_msg);
		memcpy(string, zmq_msg_data(&in_msg), size);
		zmq_msg_close(&in_msg);	
		string[size]=0;
		printf("Message received-->%s\n", string);
		//sleep(1);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}


}

void hZMQNet::set_server_name(){
//	server =str;
}

void hZMQNet::do_some_work(std::string  str,  mtQueue<std::string>& qu){
   
    void* socket;
    void* context;	
    context = zmq_ctx_new();
    socket = zmq_socket(context, ZMQ_REQ);
    
    std::cout <<"[testing]+"<< str <<  std::endl;
    std::cout << "The address" << std::endl;

    int rc = zmq_connect(socket, str.c_str());

    std::cout << rc << std::endl;

    if (rc != 0) {
	std::cout << "Can not open a socket connection" << std::endl;
	exit(1);
	}

    std::string msg;

    while (true) {
    	if (qu.size() > 0) {
		msg = qu.front();
		qu.pop();

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		std::cout << "Sending the message to the master p server" << std::endl;
    		ZMQNet::send(socket, msg);
        	std::cout << "[testing] The message has been sent to the p server" << std :: endl;
    		ZMQNet::recv(socket, msg);
    		std::cout << "receiveing the message from the master p server" << std::endl;
	}
	else {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
    }
    zmq_send(socket, nullptr, 0, 0);
    zmq_close(socket);
    zmq_ctx_term(context);



}


#endif
