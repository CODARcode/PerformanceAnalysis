#ifdef _USE_ZMQNET
#include "chimbuko/net/zmq_net.hpp"
#include "chimbuko/message.hpp"
#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/verbose.hpp"
#include "chimbuko/util/string.hpp"
#include <iostream>
#include <string.h>

#ifdef _PERF_METRIC
#include <chrono>
#include <fstream>
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MilliSec;
typedef std::chrono::microseconds MicroSec;
#endif

using namespace chimbuko;

/**
 * @brief Register client connection
 */
class NetPayloadHandShakeWithCount: public NetPayloadBase{
public:
  int &clients;
  bool &client_has_connected;
  std::mutex &m;
  NetPayloadHandShakeWithCount(int &clients, bool &client_has_connected, std::mutex &m): clients(clients), m(m), client_has_connected(client_has_connected){}
  
  MessageKind kind() const{ return MessageKind::DEFAULT; }
  MessageType type() const{ return MessageType::REQ_ECHO; }
  void action(Message &response, const Message &message) override{
    check(message);
    response.set_msg(std::string("Hello!I am NET!"), false);
    std::lock_guard<std::mutex> _(m);
    clients++;
    client_has_connected = true;
    VERBOSE(std::cout << "ZMQNet handshake received, #clients is now " << clients << std::endl);
  };
};

/**
 * @brief Register client disconnection
 */
class NetPayloadClientDisconnectWithCount: public NetPayloadBase{
public:
  int &clients;
  std::mutex &m;
  NetPayloadClientDisconnectWithCount(int &clients, std::mutex &m): clients(clients), m(m){}
  
  MessageKind kind() const{ return MessageKind::DEFAULT; }
  MessageType type() const{ return MessageType::REQ_QUIT; }
  void action(Message &response, const Message &message) override{
    check(message);
    response.set_msg("", false);
    std::lock_guard<std::mutex> _(m);
    clients--;
    if(clients < 0) throw std::runtime_error("ZMQNet registered clients < 0! Likely a client did not perform a handshake");

    VERBOSE(std::cout << "ZMQNet received disconnect notification, #clients is now " << clients << std::endl);
  };
};


ZMQNet::ZMQNet() : m_context(nullptr), m_n_requests(0), m_max_pollcyc_msg(10), m_io_threads(1), m_clients(0), m_client_has_connected(false)
{
  add_payload(new NetPayloadHandShakeWithCount(m_clients, m_client_has_connected, m_mutex));
  add_payload(new NetPayloadClientDisconnectWithCount(m_clients, m_mutex));
}

ZMQNet::~ZMQNet()
{
}

void ZMQNet::init(int* /*argc*/, char*** /*argv*/, int nt)
{
    m_context  = zmq_ctx_new();
    if(zmq_ctx_set(m_context, ZMQ_IO_THREADS, m_io_threads) != 0)
      throw std::runtime_error("ZMQNet::init couldn't set number of io threads to requested amount");   

    //Check worker_idx = 0 for all payloads
    for(auto const &e : m_payloads)
      if(e.first != 0) throw std::runtime_error("ZMQNet all payloads must have worker_idx=0");

    init_thread_pool(nt);
}

void doWork(void* context,     
	    std::unordered_map<MessageKind,
	    std::unordered_map<MessageType,  std::unique_ptr<NetPayloadBase> >
	    > &payloads,
	    PerfStats &perf, int thr_idx)
{
  void* socket = zmq_socket(context, ZMQ_REP); //create a REP socket (recv, send, recv, send pattern)
  zmq_connect(socket, "inproc://workers"); //connect the socket to the worker pool
    
  zmq_pollitem_t items[1] = { { socket, 0, ZMQ_POLLIN, 0 } }; 
 
  std::string perf_prefix = "worker" + anyToStr(thr_idx) + "_";
  PerfTimer timer;

  while(true){
    //Poll the socket to check for incoming messages. If an error is returned, exit the loop
    //Errors typically mean the context has been terminated
    constexpr int nitems = 1; //size of items array
    constexpr long timeout = -1; //poll forever until error or success
    timer.start();
    if(zmq_poll(items, nitems, timeout) < 1){
      break;
    }
    perf.add(perf_prefix + "poll_time_ms", timer.elapsed_ms());
    
    //Receive the message into a json-formatted string
    timer.start();
    std::string strmsg;
    ZMQNet::recv(socket, strmsg);
    perf.add(perf_prefix + "receive_from_front_ms", timer.elapsed_ms());

    VERBOSE(std::cout << "ZMQNet worker thread " << thr_idx << " received message of size " << strmsg.size() << std::endl);
    
    //Parse the message and instantiate a reply message with appropriate sender
    Message msg, msg_reply;
    msg.set_msg(strmsg, true);

    msg_reply = msg.createReply();
   
    timer.start();
    auto kit = payloads.find((MessageKind)msg.kind());
    if(kit == payloads.end()) throw std::runtime_error("ZMQNet::doWork : No payload associated with the message kind provided. Message: " + strmsg + " (did you add the payload to the server?)");
    auto pit = kit->second.find((MessageType)msg.type());
    if(pit == kit->second.end()) throw std::runtime_error("ZMQNet::doWork : No payload associated with the message type provided. Mesage: " + strmsg + " (did you add the payload to the server?)");
    perf.add(perf_prefix + "message_kind_type_lookup_ms", timer.elapsed_ms());

    //Apply the payload
    timer.start();
    pit->second->action(msg_reply, msg);
    strmsg = msg_reply.data();
    VERBOSE(std::cout << "ZMQNet worker thread " << thr_idx << " sending response of size " << strmsg.size() << std::endl);
    perf.add(perf_prefix + "perform_action_ms", timer.elapsed_ms());

    //Send the reply
    timer.start();
    ZMQNet::send(socket, strmsg);
    perf.add(perf_prefix + "send_to_front_ms", timer.elapsed_ms());

  }//while(<receiving messages>)
  
  zmq_close(socket);
}

void ZMQNet::init_thread_pool(int nt){
  m_perf_thr.resize(nt);

  for (int i = 0; i < nt; i++) {
    m_threads.push_back(
			std::thread(&doWork, std::ref(m_context), std::ref(m_payloads.begin()->second), std::ref(m_perf_thr[i]), i )
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

    std::string perf_prefix = "router_";
    PerfTimer timer, freq_timer;

#ifdef _PERF_METRIC
    m_perf.setWriteLocation(logdir, "ps_perf_stats.txt");

    unsigned int n_requests = 0, n_replies = 0;
    double duration, elapsed;
    std::ofstream f;

    f.open(logdir + "/ps_perf.txt", std::fstream::out | std::fstream::app);
    if (f.is_open())
        f << "# PS PERFORMANCE MEASURE" << std::endl;
    Clock::time_point t_init = Clock::now();
    Clock::time_point t_start = Clock::now();  
    Clock::time_point t_end;
#endif
    
    //For measuring receive/response frequency
    freq_timer.start();
    size_t freq_n_req = 0, freq_n_reply = 0;

    VERBOSE(std::cout << "ZMQnet starting polling" << std::endl);
    m_n_requests = 0;
    while(true){
      if(m_client_has_connected && m_clients == 0){
	if(m_n_requests == 0){
	  VERBOSE(std::cout << "ZMQnet all clients have disconnected and queue cleared" << std::endl);
	  break;
	}else{
	  VERBOSE(std::cout << "ZMQnet all clients have disconnected, waiting for queue clearance" << std::endl);	  
	}
      }

      timer.start();
      int err = zmq_poll(items, NR_ITEMS, -1);  //wait (indefinitely) for comms on both sockets
      if(err == -1){
	std::string error = std::string("ZMQnet::run polling failed with error: ") + strerror(errno) + "\n";
	throw std::runtime_error(error);
      }
      m_perf.add(perf_prefix + "poll_time_ms", timer.elapsed_ms());

      VERBOSE(std::cout << "ZMQnet received message" << std::endl);

      //If the message was received from a worker thread, route to the AD
      //Drain the outgoing queue first to free resources
      if(items[1].revents & ZMQ_POLLIN) {
	timer.start();
	int nmsg = recvAndSend(backend, frontend, m_max_pollcyc_msg);
	m_perf.add(perf_prefix + "route_back_to_front_ms", timer.elapsed_ms());
	VERBOSE(std::cout << "ZMQnet routed " << nmsg << " messages from back to front\n");
	m_n_requests -= nmsg; //decrement number of outstanding requests
#ifdef _PERF_METRIC
	m_perf.add(perf_prefix + "route_front_to_back_msg_per_cyc", nmsg);
	freq_n_reply += nmsg;
	n_replies += nmsg;
#endif
      } 
      
      //If the message was received from the AD, route the message to a worker thread
      if(items[0].revents & ZMQ_POLLIN) { 
	timer.start();
	int nmsg = recvAndSend(frontend, backend, m_max_pollcyc_msg);
	m_perf.add(perf_prefix + "route_front_to_back_ms", timer.elapsed_ms());
	VERBOSE(std::cout << "ZMQnet routed " << nmsg << " messages from front to back\n");
	m_n_requests += nmsg;
#ifdef _PERF_METRIC
	m_perf.add(perf_prefix + "route_back_to_front_msg_per_cyc", nmsg);
	freq_n_req += nmsg;
	n_requests += nmsg;
#endif
      }


#ifdef _PERF_METRIC
      t_end = Clock::now();
      duration = double(std::chrono::duration_cast<MicroSec>(t_end - t_start).count())/1000.;
      if (duration >= 10000. && f.is_open()) {
	elapsed = double(std::chrono::duration_cast<MicroSec>(t_end - t_init).count())/1000.;
	f << elapsed << " " 
	  << n_requests << " " 
	  << n_replies << " " 
	  << duration 
	  << std::endl;
	t_start = t_end;
	n_requests = 0;
	n_replies = 0;
      }

      double freq_elapsed = freq_timer.elapsed_ms();
      if(freq_elapsed > 1000){
	m_perf.add(perf_prefix + "receive_rate_in_per_ms", double(freq_n_req)/freq_elapsed );
	m_perf.add(perf_prefix + "response_rate_in_per_ms", double(freq_n_reply)/freq_elapsed );
	freq_n_req = freq_n_reply = 0; freq_timer.start();
      }
#endif
    }

    //Force perf output when exiting
#ifdef _PERF_METRIC
      t_end = Clock::now();
      duration = double(std::chrono::duration_cast<MicroSec>(t_end - t_start).count())/1000.;
      if (f.is_open()) {
	elapsed = double(std::chrono::duration_cast<MicroSec>(t_end - t_init).count())/1000.;
	f << elapsed << " " 
	  << n_requests << " " 
	  << n_replies << " " 
	  << duration
	  << std::endl;
      }

      double freq_elapsed = freq_timer.elapsed_ms();
      m_perf.add(perf_prefix + "receive_rate_in_per_ms", double(freq_n_req)/freq_elapsed );
      m_perf.add(perf_prefix + "response_rate_in_per_ms", double(freq_n_reply)/freq_elapsed );

      //Combine and write out statistics
      for(auto const &t : m_perf_thr)
	m_perf += t;
      m_perf.write();
#endif

    //Close the sockets; this will end the worker threads
    zmq_close(frontend);
    zmq_close(backend);

#ifdef _PERF_METRIC
    if (f.is_open())
        f.close();
#endif
}


int ZMQNet::recvAndSend(void* skFrom, void* skTo, int max_msg){
  //Returns number of messages routed (i.e. excluding disconnect message)
  size_t more_size = sizeof(int);
  int msg_count = 0;

  while(msg_count < max_msg){
    zmq_msg_t msg;
    zmq_msg_init(&msg);

    int len = zmq_msg_recv(&msg, skFrom, ZMQ_DONTWAIT); //non-blocking receive
    if(len == -1 && errno == EAGAIN){ //no more messages on queue
      zmq_msg_close(&msg);
      return msg_count;
    }else if(len == -1) throw std::runtime_error("ZMQNet::recvAndSend error receiving: " + std::string(zmq_strerror(errno)));
    int more;
    zmq_getsockopt(skFrom, ZMQ_RCVMORE, &more, &more_size); //check whether message is part of a multi-part message

    if(!more)
      ++msg_count; //don't count multi-part messages as distinct messages to avoid exiting too early
    
    if(more || len)
      zmq_msg_send(&msg, skTo, more ? ZMQ_SNDMORE: 0);

    zmq_msg_close(&msg);
  }

  return msg_count;
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
