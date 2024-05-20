#include "chimbuko/core/net/zmq_net.hpp"
#ifdef _USE_ZMQNET
#include "chimbuko/core/message.hpp"
#include "chimbuko/core/verbose.hpp"
#include "chimbuko/core/util/string.hpp"
#include "chimbuko/core/util/error.hpp"
#include <iostream>
#include <string.h>
#include <set>

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

  int kind() const override{ return BuiltinMessageKind::DEFAULT; }
  MessageType type() const override{ return MessageType::REQ_ECHO; }
  void action(Message &response, const Message &message) override{
    check(message);
    response.setContent(std::string("Hello!I am NET!"));
    std::lock_guard<std::mutex> _(m);
    clients++;
    client_has_connected = true;
    verboseStream << "ZMQNet handshake received, #clients is now " << clients << std::endl;
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

  int kind() const override{ return BuiltinMessageKind::DEFAULT; }
  MessageType type() const override{ return MessageType::REQ_QUIT; }
  void action(Message &response, const Message &message) override{
    check(message);
    response.setContent("");
    std::lock_guard<std::mutex> _(m);
    clients--;
    if(clients < 0) fatal_error("ZMQNet registered clients < 0! Likely a client did not perform a handshake");

    verboseStream << "ZMQNet received disconnect notification, #clients is now " << clients << std::endl;
  };
};


/**
 * @brief Allow the client to request the pserver stop
 */
class NetPayloadClientRemoteStop: public NetPayloadBase{
public:
  bool &register_stop_cmd;
  std::mutex &m;
  NetPayloadClientRemoteStop(bool &register_stop_cmd, std::mutex &m): register_stop_cmd(register_stop_cmd), m(m){}
  
  int kind() const override{ return BuiltinMessageKind::CMD; }
  MessageType type() const override{ return MessageType::REQ_QUIT; }
  void action(Message &response, const Message &message) override{
    check(message);
    response.setContent("");
    std::lock_guard<std::mutex> _(m);
    register_stop_cmd = true;
    verboseStream << "ZMQNet received remote stop command" << std::endl;
  };
};



ZMQNet::ZMQNet() : m_context(nullptr), m_n_requests(0), m_max_pollcyc_msg(10), m_io_threads(1), m_clients(0), m_client_has_connected(false), m_port(5559), m_status(ZMQNet::Status::NotStarted), m_autoshutdown(true), m_poll_timeout(-1), m_remote_stop_cmd(false)
{}

ZMQNet::~ZMQNet()
{
}

void ZMQNet::init(int* /*argc*/, char*** /*argv*/, int nt)
{
    m_context  = zmq_ctx_new();
    if(zmq_ctx_set(m_context, ZMQ_IO_THREADS, m_io_threads) != 0)
      fatal_error("ZMQNet::init couldn't set number of io threads to requested amount");

    //Because ZMQNet workers are interchangeable, added a check to ensure all workers have a work item associated with a given message kind/type
    std::set<std::pair<int, MessageType> > mtypes;
    for(auto const &t: m_payloads)
      for(auto const &k : t.second)
	for(auto const &t: k.second)
	  mtypes.insert(std::pair<int, MessageType>(k.first,t.first));
    if(mtypes.size()){ //only do the check if work items have actually been assigned
      for(int t=0; t<nt; t++){
	auto thrit = m_payloads.find(t);
	if(thrit == m_payloads.end()) fatal_error(stringize("Thread %d does not have any payloads!", t));
	auto const &tloads = thrit->second;

	for(auto const &kt: mtypes){
	  verboseStream << "ZMQNet::init checking thread " << t << " payloads for " << kt.first << "," << toString(kt.second) << std::endl;
	  auto kit = tloads.find(kt.first);
	  if(kit == tloads.end()) fatal_error(stringize("Thread %d does not have a payload for message kind %d",t,kt.first));
	  auto tit = kit->second.find(kt.second);
	  if(tit == kit->second.end()) fatal_error(stringize("Thread %d does not have a payload for message type %s",t,toString(kt.second).c_str()));
	}
      }
    }

    //Add default work to all threads
    for(int t=0;t<nt;t++){
      add_payload(new NetPayloadHandShakeWithCount(m_clients, m_client_has_connected, m_mutex),t);
      add_payload(new NetPayloadClientDisconnectWithCount(m_clients, m_mutex),t);
      add_payload(new NetPayloadClientRemoteStop(m_remote_stop_cmd, m_mutex),t);
    }

    init_thread_pool(nt);
}

void doWork(void* context,
	    std::unordered_map<int,
	    std::unordered_map<MessageType,  std::unique_ptr<NetPayloadBase> >
	    > &payloads,
	    PerfStats &perf, int thr_idx,
	    std::mutex &thr_mutex)
{
  void* socket = zmq_socket(context, ZMQ_REP); //create a REP socket (recv, send, recv, send pattern)

  int zero = 0;
  zmq_setsockopt(socket, ZMQ_RCVHWM, &zero, sizeof(int));
  zmq_setsockopt(socket, ZMQ_SNDHWM, &zero, sizeof(int));

  zmq_connect(socket, "inproc://workers"); //connect the socket to the worker pool

  zmq_pollitem_t items[1] = { { socket, 0, ZMQ_POLLIN, 0 } };

  std::string perf_prefix = "worker" + anyToStr(thr_idx) + "_";
  PerfTimer timer;

  while(true){
    //Poll the socket to check for incoming messages. If an error is returned, exit the loop
    //Errors typically mean the context has been terminated
    constexpr int nitems = 1; //size of items array
    constexpr long timeout = -1; //poll forever until error or success

    double poll_time, recv_time, parse_time, perf_time, send_time;

    timer.start();
    if(zmq_poll(items, nitems, timeout) < 1){
      break;
    }
    poll_time = timer.elapsed_ms();

    std::string strmsg;

    //Receive the message
    timer.start();
    ZMQNet::recv(socket, strmsg);
    recv_time = timer.elapsed_ms();

    verboseStream << "ZMQNet worker thread " << thr_idx << " received message of size " << strmsg.size() << std::endl;

    //Parse the message and instantiate a reply message with appropriate sender
    timer.start();
    Message msg, msg_reply;
    msg.deserializeMessage(strmsg);
    parse_time = timer.elapsed_ms();

    int kind = msg.kind();
    MessageType type = (MessageType)msg.type();

    timer.start();
    NetInterface::find_and_perform_action(msg_reply, msg, payloads);
    perf_time = timer.elapsed_ms();

    //Send the reply
    strmsg = msg_reply.serializeMessage();
    verboseStream << "ZMQNet worker thread " << thr_idx << " sending response of size " << strmsg.size() << std::endl;

    timer.start();
    ZMQNet::send(socket, strmsg);
    send_time = timer.elapsed_ms();

    std::lock_guard<std::mutex> _(thr_mutex);   
    perf.add(perf_prefix + "poll_time_ms", poll_time);
    perf.add(perf_prefix + "receive_from_front_ms", recv_time);
    perf.add(perf_prefix + "message_parse_ms", parse_time);
    perf.add(perf_prefix + "find_and_perform_action_kind"+std::to_string(kind)+"_"+toString(type)+"_ms", perf_time);
    perf.add(perf_prefix + "send_to_front_ms", send_time);
  }//while(<receiving messages>)

  zmq_close(socket);
}

void ZMQNet::init_thread_pool(int nt){
  m_perf_thr.resize(nt);
  {
    std::vector<std::mutex> tmp(nt);
    m_thr_mutex.swap(tmp);
  }

  for (int i = 0; i < nt; i++) {
    auto pit = m_payloads.find(i);
    if(pit == m_payloads.end()) fatal_error("Could not find work for thread");
    m_threads.push_back(
			std::thread(&doWork, std::ref(m_context), std::ref(pit->second), std::ref(m_perf_thr[i]), i, std::ref(m_thr_mutex[i]) )
			);
  }
}

void ZMQNet::stop(){
  //Send a stop command on the stop socket, presumably from a different thread
  if(m_status == Status::Running){
    void* socket = zmq_socket(m_context, ZMQ_REQ);
    zmq_connect(socket, "inproc://shutdown"); //connect the socket to the worker pool
    zmq_send(socket, nullptr, 0, 0);
    zmq_close(socket);
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
  progressStream << "PServer ZMQNet start" << std::endl;
  m_status = Status::StartingUp;

  void* stop_socket = zmq_socket(m_context, ZMQ_REP);
  void* frontend = zmq_socket(m_context, ZMQ_ROUTER);
  void* backend  = zmq_socket(m_context, ZMQ_DEALER);

  int zero = 0;
  zmq_setsockopt(frontend, ZMQ_RCVHWM, &zero, sizeof(int));
  zmq_setsockopt(frontend, ZMQ_SNDHWM, &zero, sizeof(int));
  zmq_setsockopt(backend, ZMQ_RCVHWM, &zero, sizeof(int));
  zmq_setsockopt(backend, ZMQ_SNDHWM, &zero, sizeof(int));

  std::string tcp_addr = "tcp://*:"+anyToStr(m_port);
  zmq_bind(frontend, tcp_addr.c_str()); //create a socket for communication with the AD
  zmq_bind(backend, "inproc://workers"); //create a socket for distributing work among the worker threads
  zmq_bind(stop_socket, "inproc://shutdown"); //create a socket for communicating shutdown messages

  const int NR_ITEMS = 3;
  zmq_pollitem_t items[NR_ITEMS] =
    {
      { frontend, 0, ZMQ_POLLIN, 0 },
      { backend , 0, ZMQ_POLLIN, 0 },
      { stop_socket, 0, ZMQ_POLLIN, 0 }
    };

  std::string perf_prefix = "router_";
  PerfTimer timer, freq_timer, heartbeat_timer;

#ifdef _PERF_METRIC
  m_perf.setWriteLocation(logdir, "ps_perf_stats.json");

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

  m_remote_stop_cmd = false;
  m_status = Status::Running;
  Status stop_status; //record why it stops

  //For measuring receive/response frequency
  freq_timer.start();
  size_t freq_n_req = 0, freq_n_reply = 0;
  heartbeat_timer.start();

  verboseStream << "ZMQnet starting polling" << std::endl;
  m_n_requests = 0;
  while(true){

    //Autoshutdown or action of remote stop
    //Note: the server will only finish if all the clients have disconnected
    if( (m_autoshutdown || m_remote_stop_cmd) && m_client_has_connected && m_clients == 0){
      if(m_n_requests == 0){
	verboseStream << "ZMQnet all clients have disconnected and queue cleared" << std::endl;
	stop_status = m_remote_stop_cmd ? Status::StoppedByRequest : Status::StoppedAutomatically;
	break;
      }else{
	verboseStream << "ZMQnet all clients have disconnected, waiting for queue clearance" << std::endl;
      }
    }

    timer.start();
    int err = zmq_poll(items, NR_ITEMS, m_poll_timeout);
    m_perf.add(perf_prefix + "poll_time_ms", timer.elapsed_ms());

    //If we either timeout or encounter an error we gracefully exit
    if(err == 0){ //timed out
      recoverable_error(std::string("ZMQnet::run polling timed out after ") + anyToStr(m_poll_timeout) + " ms\n");
      stop_status = Status::StoppedByTimeOut;
      break;
    }else if(err == -1){ //actual error
      if(errno == EINTR){
	std::cout << "ZMQNet received system signal to stop, shutting down" << std::endl;
	stop_status = Status::StoppedBySignal;
      }else{
	recoverable_error(std::string("ZMQnet::run polling failed with error: ") + strerror(errno) + "\n");
	stop_status = Status::StoppedByError;
      }
      break;
    }

    verboseStream << "ZMQnet received message" << std::endl;

    //If the message was received from a worker thread, route to the AD
    //Drain the outgoing queue first to free resources
    if(items[1].revents & ZMQ_POLLIN) {
      timer.start();
      int nmsg = recvAndSend(backend, frontend, m_max_pollcyc_msg);
      m_perf.add(perf_prefix + "route_back_to_front_ms", timer.elapsed_ms());
      verboseStream << "ZMQnet routed " << nmsg << " messages from back to front\n";
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
      verboseStream << "ZMQnet routed " << nmsg << " messages from front to back\n";
      m_n_requests += nmsg;
#ifdef _PERF_METRIC
      m_perf.add(perf_prefix + "route_back_to_front_msg_per_cyc", nmsg);
      freq_n_req += nmsg;
      n_requests += nmsg;
#endif
    }

    //Stop if asked
    if(items[2].revents & ZMQ_POLLIN){
      zmq_recv(stop_socket, nullptr, 0,0);
      std::cout << "ZMQNet received signal to stop" << std::endl;
      stop_status = Status::StoppedByRequest;
      break;
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
    if(heartbeat_timer.elapsed_ms() > 10000){ //flush the perf statistics in case the server falls over
      progressStream << "PServer heartbeat" << std::endl;
      for(size_t i=0;i< m_perf_thr.size(); i++){
	std::lock_guard<std::mutex> _(m_thr_mutex[i]);	
	m_perf += m_perf_thr[i];
	m_perf_thr[i].clear();
      }
      m_perf.write();
      heartbeat_timer.start();
    }
#endif
  }

  m_status = Status::ShuttingDown;

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
  for(size_t i=0;i< m_perf_thr.size(); i++){
    std::lock_guard<std::mutex> _(m_thr_mutex[i]);	
    m_perf += m_perf_thr[i];
  }
  m_perf.write();
#endif

  //Close the sockets; this will end the worker threads
  zmq_close(frontend);
  zmq_close(backend);
  zmq_close(stop_socket);

  m_status = stop_status;

#ifdef _PERF_METRIC
  if (f.is_open())
    f.close();
#endif
  progressStream << "PServer ZMQNet finished" << std::endl;
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
    }else if(len == -1) fatal_error("ZMQNet::recvAndSend error receiving: " + std::string(zmq_strerror(errno)));
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

    if(ret == -1) fatal_error("Send failed with error: " + std::string(zmq_strerror(errno)) );

    return ret;
}

int ZMQNet::recv(void* socket, std::string& strmsg)
{
    zmq_msg_t msg;
    int ret;
    zmq_msg_init(&msg);
    ret = zmq_msg_recv(&msg, socket, 0);
    if(ret != -1) strmsg.assign((char*)zmq_msg_data(&msg), ret);
    zmq_msg_close(&msg);

    if(ret == -1) fatal_error("Receive failed with error: " + std::string(zmq_strerror(errno)) );

    return ret;
}
#endif
