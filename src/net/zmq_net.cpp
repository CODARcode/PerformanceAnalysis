#ifdef _USE_ZMQNET
#include "chimbuko/net/zmq_net.hpp"
#include "chimbuko/message.hpp"
#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/verbose.hpp"
#include <iostream>
#include <string.h>

#ifdef _PERF_METRIC
#include <chrono>
#include <fstream>
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MilliSec;
#endif

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

void doWork(void* context,     
	    std::unordered_map<MessageKind,
	    std::unordered_map<MessageType,  std::unique_ptr<NetPayloadBase> >
	    > &payloads)
{
  void* socket = zmq_socket(context, ZMQ_REP); //create a REP socket (recv, send, recv, send pattern)
  zmq_connect(socket, "inproc://workers"); //connect the socket to the worker pool
    
  zmq_pollitem_t items[1] = { { socket, 0, ZMQ_POLLIN, 0 } }; 
 
  while(true){

    //Poll the socket to check for incoming messages. If an error is returned, exit the loop
    //Errors typically mean the context has been terminated
    constexpr int nitems = 1; //size of items array
    constexpr long timeout = -1; //poll forever until error or success
    if(zmq_poll(items, nitems, timeout) < 1){
      break;
    }

    //Receive the message into a json-formatted string
    std::string strmsg;
    ZMQNet::recv(socket, strmsg);

    VERBOSE(std::cout << "ZMQ worker received message: " << strmsg << std::endl);
    
    //Parse the message and instantiate a reply message with appropriate sender
    Message msg, msg_reply;
    msg.set_msg(strmsg, true);

    msg_reply = msg.createReply();

    auto kit = payloads.find((MessageKind)msg.kind());
    if(kit == payloads.end()) throw std::runtime_error("ZMQNet::doWork : No payload associated with the message kind provided (did you add the payload to the server?)");
    auto pit = kit->second.find((MessageType)msg.type());
    if(pit == kit->second.end()) throw std::runtime_error("ZMQNet::doWork : No payload associated with the message type provided (did you add the payload to the server?)");

    //Apply the payload
    pit->second->action(msg_reply, msg);
    VERBOSE(std::cout << "Worker sending response: " << msg_reply.data() << std::endl);
    
    //Send the reply
    ZMQNet::send(socket, msg_reply.data());
    
  }//while(<receiving messages>)
  
  zmq_close(socket);
}

void ZMQNet::init_thread_pool(int nt)
{
    for (int i = 0; i < nt; i++) {
        m_threads.push_back(
			    std::thread(&doWork, std::ref(m_context), std::ref(m_payloads) )
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

#ifdef _PERF_METRIC
void ZMQNet::run(std::string logdir)
#else
void ZMQNet::run()
#endif
{    
    void* frontend = zmq_socket(m_context, ZMQ_ROUTER);
    void* backend  = zmq_socket(m_context, ZMQ_DEALER);

    zmq_bind(frontend, "tcp://*:5559"); //create a socket for communication with the AD
    zmq_bind(backend, "inproc://workers"); //create a socket for distributing work among the worker threads

    const int NR_ITEMS = 2; 
    zmq_pollitem_t items[NR_ITEMS] = 
    {
        { frontend, 0, ZMQ_POLLIN, 0 },
        { backend , 0, ZMQ_POLLIN, 0 }
    };

#ifdef _PERF_METRIC
    unsigned int n_requests = 0, n_replies = 0;
    Clock::time_point t_start, t_end, t_init;
    MilliSec duration, elapsed;
    std::ofstream f;

    f.open(logdir + "/ps_perf.txt", std::fstream::out | std::fstream::app);
    if (f.is_open())
        f << "# PS PERFORMANCE MEASURE" << std::endl;
    t_init = Clock::now();
    t_start = Clock::now();  
#endif

    VERBOSE(std::cout << "ZMQnet starting polling" << std::endl);
    m_n_requests = 0;
    while(true){
      int err = zmq_poll(items, NR_ITEMS, -1);  //wait (indefinitely) for comms on both sockets
      if(err == -1){
	std::string error = std::string("ZMQnet::run polling failed with error: ") + strerror(errno) + "\n";
	throw std::runtime_error(error);
      }
      VERBOSE(std::cout << "ZMQnet received message" << std::endl);
      
      //If the message was received from the AD, route the message to a worker thread
      if(items[0].revents & ZMQ_POLLIN) { 
	if (recvAndSend(frontend, backend)) {
	  stop();
	  break;
	}
	m_n_requests++;
#ifdef _PERF_METRIC
	n_requests++;
#endif
      }

      //If the message was received from a worker thread, route to the AD
      if(items[1].revents & ZMQ_POLLIN) {
	recvAndSend(backend, frontend);
	m_n_requests--;
#ifdef _PERF_METRIC
	n_replies++;
#endif
      } 

#ifdef _PERF_METRIC
      t_end = Clock::now();
      duration = std::chrono::duration_cast<MilliSec>(t_end - t_start);
      if (duration.count() >= 10000 && f.is_open()) {
	elapsed = std::chrono::duration_cast<MilliSec>(t_end - t_init);
	f << elapsed.count() << " " 
	  << n_requests << " " 
	  << n_replies << " " 
	  << duration.count() 
	  << std::endl;
	t_start = t_end;
	n_requests = 0;
	n_replies = 0;
      }
#endif
    }

    //Close the sockets
    zmq_close(frontend);
    zmq_close(backend);

#ifdef _PERF_METRIC
    if (f.is_open())
        f.close();
#endif
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
