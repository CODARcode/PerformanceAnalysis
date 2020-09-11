#ifdef _USE_ZMQNET
#include "chimbuko/net/zmq_net.hpp"
#include "chimbuko/net/zmqme_net.hpp"
#include "chimbuko/message.hpp"
#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/verbose.hpp"
#include "chimbuko/util/string.hpp"
#include <iostream>
#include <sstream>
#include <string.h>

#ifdef _PERF_METRIC
#include <chrono>
#include <fstream>
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MilliSec;
typedef std::chrono::microseconds MicroSec;
#endif

using namespace chimbuko;

ZMQMENet::ZMQMENet() : m_base_port(5559), m_exit(false), m_finalized(true){
}

ZMQMENet::~ZMQMENet()
{
}

void ZMQMENet::init(int* /*argc*/, char*** /*argv*/, int nt){
  m_nt = nt;
  for(int t=0;t<m_nt;t++)
    add_payload(new NetPayloadHandShake, t);
}

void doWork(std::unordered_map<int,
	    std::unordered_map<MessageKind,
	    std::unordered_map<MessageType,  std::unique_ptr<NetPayloadBase> > 
	    >
	    > &payloads,
	    PerfStats &perf, int thr_idx, int port, std::atomic<bool> &exit)
{
  auto it = payloads.find(thr_idx);
  if(it == payloads.end()) return; //thread has no work to do
  
  auto &payloads_t = it->second;

  void* context = zmq_ctx_new();
  void* socket = zmq_socket(context, ZMQ_REP); //create a REP socket (recv, send, recv, send pattern)

  std::stringstream addr;
  addr << "tcp://*:" << port;

  VERBOSE(std::cout << "ZMQMENet worker thread " << thr_idx << " binding to " << addr.str() << std::endl);

  zmq_bind(socket, addr.str().c_str()); //create a socket for communication with the AD

  constexpr int nitems = 1; //size of items array
  constexpr long timeout = -1; //poll forever until error or success
  zmq_pollitem_t items[nitems] = { { socket, 0, ZMQ_POLLIN, 0 } }; 
 
  std::string perf_prefix = "worker" + anyToStr(thr_idx) + "_";
  PerfTimer timer;

  bool exit_signal_received = false; //breakout flag, client has indicated that the worker should terminate

  while(true){
    timer.start();

    //Check if the exit flag has been set either by the client or by the server, if so exit now
    if(exit.load() || exit_signal_received)
      break;

    //Poll the socket to check for incoming messages
    if(zmq_poll(items, nitems, timeout) < 1){ //because we block indefinitely zmq_poll return value of <= 0 is an error
      break;
    }
    perf.add(perf_prefix + "poll_time_ms", timer.elapsed_ms());

    while(true){
      //Receive the message into a json-formatted string
      timer.start();
      std::string strmsg;
      int ret = ZMQMENet::recv_NB(socket, strmsg);
      if(ret == -1) 
	break; //no messages on queue
      perf.add(perf_prefix + "receive_ms", timer.elapsed_ms());

      if(ret == 0){ //zero-length message used as terminate signal
	VERBOSE(std::cout << "ZMQME worker thread " << thr_idx << " received exit signal\n");
	exit_signal_received = true;
	break;
      }
      
      VERBOSE(std::cout << "ZMQME worker thread " << thr_idx << " received message: " << strmsg << std::endl);
    
      //Parse the message and instantiate a reply message with appropriate sender
      Message msg, msg_reply;
      msg.set_msg(strmsg, true);
      
      msg_reply = msg.createReply();
      
      timer.start();
      auto kit = payloads_t.find((MessageKind)msg.kind());
      if(kit == payloads_t.end()) throw std::runtime_error("ZMQMENet::doWork : No payload associated with the message kind provided (did you add the payload to the server?)");
      auto pit = kit->second.find((MessageType)msg.type());
      if(pit == kit->second.end()) throw std::runtime_error("ZMQMENet::doWork : No payload associated with the message type provided (did you add the payload to the server?)");
      perf.add(perf_prefix + "message_kind_type_lookup_ms", timer.elapsed_ms());

      //Apply the payload
      timer.start();
      pit->second->action(msg_reply, msg);
      VERBOSE(std::cout << "ZMQMENet Worker thread " << thr_idx << " sending response: " << msg_reply.data() << std::endl);
      perf.add(perf_prefix + "perform_action_ms", timer.elapsed_ms());

      //Send the reply
      timer.start();
      ZMQNet::send(socket, msg_reply.data());
      perf.add(perf_prefix + "send_to_front_ms", timer.elapsed_ms());
    } //while(<work in queue>)

  }//while(<receiving messages>)
  
  zmq_close(socket);
  zmq_ctx_term(context);
}

void ZMQMENet::finalize()
{
  if(m_finalized) 
    return;

  //Signal the threads to exit
  m_exit.store(true);
  //Join threads once they have terminated
  for (auto& t: m_threads)
    if (t.joinable())
      t.join();
  m_threads.clear();

#ifdef _PERF_METRIC
  //Combine and write out statistics
  for(auto const &t : m_perf_thr)
    m_perf += t;
  m_perf.write();
#endif
  m_perf_thr.clear();

  m_finalized = true;
  VERBOSE(std::cout << "ZMQMENet finalize completed" << std::endl);
}

void ZMQMENet::init_thread_pool(int nt){
  finalize(); //finalize if restarting and not already done previously
  m_perf_thr.resize(m_nt);
  for (int i = 0; i < m_nt; i++) {
    m_threads.push_back(
			std::thread(&doWork, std::ref(m_payloads), std::ref(m_perf_thr[i]), i, m_base_port + i, std::ref(m_exit))
			);
  }
  m_finalized = false; //threads need to be joined
}

#ifdef _PERF_METRIC
void ZMQMENet::run(std::string logdir)
#else
void ZMQMENet::run()
#endif
{      
  if(m_nt == 0) throw std::runtime_error("ZMQMENet run called prior to initialization!");
  init_thread_pool(m_nt);
}

void ZMQMENet::stop(){
  finalize();
}


int ZMQMENet::recv_NB(void* socket, std::string& strmsg){
  zmq_msg_t msg;
  zmq_msg_init(&msg);
  int ret = zmq_msg_recv(&msg, socket, ZMQ_DONTWAIT);
  if(ret == -1 && errno == EAGAIN){ //no more messages in queue
    zmq_msg_close(&msg);
    return -1;
  }else if(ret == -1)
    throw std::runtime_error("ZMQMENet::recv_NB zmq_msg_recv threw an error");

  strmsg.assign((char*)zmq_msg_data(&msg), ret);
  zmq_msg_close(&msg);
  return ret;
}


#endif
