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

ZMQMENet::ZMQMENet() : m_base_port(5559), m_finalized(true){
}

ZMQMENet::~ZMQMENet()
{
}

class NetPayloadHandShakeWithCountME: public NetPayloadBase{
public:
  int &clients;
  int &client_has_connected;
  int thr_idx;

  NetPayloadHandShakeWithCountME(int thr_idx, int &clients, int &client_has_connected): clients(clients), thr_idx(thr_idx), client_has_connected(client_has_connected){}
  
  MessageKind kind() const{ return MessageKind::DEFAULT; }
  MessageType type() const{ return MessageType::REQ_ECHO; }
  void action(Message &response, const Message &message) override{
    check(message);
    response.set_msg(std::string("Hello!I am NET!"), false);
    clients++;
    client_has_connected = 1;
    VERBOSE(std::cout << "ZMQMENet thread " << thr_idx << " received handshake, #clients is now " << clients << std::endl);
  };
};

class NetPayloadClientDisconnectWithCountME: public NetPayloadBase{
public:
  int &clients;
  int thr_idx;
  NetPayloadClientDisconnectWithCountME(int thr_idx, int &clients): clients(clients), thr_idx(thr_idx){}
  
  MessageKind kind() const{ return MessageKind::DEFAULT; }
  MessageType type() const{ return MessageType::REQ_QUIT; }
  void action(Message &response, const Message &message) override{
    check(message);
    response.set_msg("", false);
    clients--;
    if(clients < 0) throw std::runtime_error("ZMQMENet registered clients < 0! Likely a client did not perform a handshake");
    VERBOSE(std::cout << "ZMQMENet thread " << thr_idx << " received disconnect notification, #clients is now " << clients << std::endl);
  };
};



void ZMQMENet::init(int* /*argc*/, char*** /*argv*/, int nt){
  m_nt = nt;
  m_clients_thr = std::vector<int>(m_nt,0);
  m_client_has_connected = std::vector<int>(m_nt, 0);

  for(int t=0;t<m_nt;t++){
    add_payload(new NetPayloadHandShakeWithCountME(t, m_clients_thr[t], m_client_has_connected[t]), t);
    add_payload(new NetPayloadClientDisconnectWithCountME(t, m_clients_thr[t]), t);
  }
}

void doWork(std::unordered_map<int,
	    std::unordered_map<MessageKind,
	    std::unordered_map<MessageType,  std::unique_ptr<NetPayloadBase> > 
	    >
	    > &payloads,
	    PerfStats &perf, int thr_idx, int port, int &clients, int &client_has_connected)
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
  zmq_pollitem_t items[nitems] = { { socket, 0, ZMQ_POLLIN, 0 } }; 
 
  std::string perf_prefix = "worker" + anyToStr(thr_idx) + "_";
  PerfTimer timer;

  while(true){
    //Check if the exit flag has been set either by the client or by the server, if so exit now
    if(client_has_connected && clients == 0){
      VERBOSE(std::cout << "ZMQMENet worker thread " << thr_idx << " exiting poll loop because all clients disconnected" << std::endl);    
      break;
    }

    timer.start();

    //Poll the socket to check for incoming messages
    int poll_ret = zmq_poll(items, nitems, -1);
    if(poll_ret == 0) throw std::runtime_error("ZMQMENet worker thread " + anyToStr(thr_idx) + " poll returned 0 despite indefinite timeout!");
    else if(poll_ret == -1) throw std::runtime_error("ZMQMENet worker thread " + anyToStr(thr_idx) + " poll received error " + zmq_strerror(errno));

    perf.add(perf_prefix + "poll_time_ms", timer.elapsed_ms());

    while(true){
      //Receive the message into a json-formatted string
      timer.start();
      std::string strmsg;
      int ret = ZMQMENet::recv_NB(socket, strmsg);
      if(ret == -1) 
	break; //no messages on queue
      perf.add(perf_prefix + "receive_ms", timer.elapsed_ms());
     
      VERBOSE(std::cout << "ZMQMENet worker thread " << thr_idx << " received message of size " << strmsg.size() << std::endl);
    
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
      VERBOSE(std::cout << "ZMQMENet Worker thread " << thr_idx << " sending response of size " << msg_reply.data().size() << std::endl);
      perf.add(perf_prefix + "perform_action_ms", timer.elapsed_ms());

      //Send the reply
      timer.start();
      ZMQNet::send(socket, msg_reply.data());
      perf.add(perf_prefix + "send_to_front_ms", timer.elapsed_ms());
    } //while(<work in queue>)

  }//while(<receiving messages>)

  VERBOSE(std::cout << "ZMQMENet worker thread " << thr_idx << " is exiting" << std::endl);
  zmq_close(socket);
  zmq_ctx_term(context);
  VERBOSE(std::cout << "ZMQMENet worker thread " << thr_idx << " has finished" << std::endl);
}

void ZMQMENet::finalize()
{
  if(m_finalized) 
    return;

  //Join threads once they have terminated
  int tidx=0;
  for(auto& t: m_threads){
    if (t.joinable()){
      VERBOSE(std::cout << "ZMQMENet joining thread " << tidx << std::endl);
      t.join();
      VERBOSE(std::cout << "ZMQMENet joined thread " << tidx << std::endl);
    }
    ++tidx;
  }

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
			std::thread(&doWork, std::ref(m_payloads), std::ref(m_perf_thr[i]), i, m_base_port + i, std::ref(m_clients_thr[i]), std::ref(m_client_has_connected[i]) )
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
#ifdef _PERF_METRIC
    m_perf.setWriteLocation(logdir, "ps_perf_stats.txt");
#endif
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
