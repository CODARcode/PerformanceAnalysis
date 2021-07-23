#include <chimbuko/ad/ADNetClient.hpp>
#include <chimbuko/verbose.hpp>
#include <chimbuko/util/error.hpp>
#include <mpi.h>

using namespace chimbuko;

ADNetClient::ADNetClient() : m_use_ps(false), m_perf(nullptr){}

ADNetClient::~ADNetClient(){}

void ADNetClient::send_and_receive(Message &recv,  const Message &send){
  recv.set_msg( send_and_receive(send), true );
}

void ADNetClient::async_send(const Message &send){
  send_and_receive(send);
}


#ifdef _USE_ZMQNET

ADZMQNetClient::ADZMQNetClient(): ADNetClient(){
  m_context = nullptr;
  m_socket = nullptr;
  m_recv_timeout_ms = 30000;
}

ADZMQNetClient::~ADZMQNetClient(){
  disconnect_ps();
}

void ADZMQNetClient::connect_ps(int rank, int srank, std::string sname) {
  m_rank = rank;
  m_srank = srank;

  m_context = zmq_ctx_new();
  m_socket = zmq_socket(m_context, ZMQ_REQ);

  int zero = 0;
  zmq_setsockopt(m_socket, ZMQ_RCVHWM, &zero, sizeof(int));
  zmq_setsockopt(m_socket, ZMQ_SNDHWM, &zero, sizeof(int));
  zmq_setsockopt(m_socket, ZMQ_RCVTIMEO, &m_recv_timeout_ms, sizeof(int));

  if(zmq_connect(m_socket, sname.c_str()) == -1){
    std::string err = strerror(errno);
    throw std::runtime_error("ZMQ failed to connect to socket at address " + sname + ", due to error: " + err);
  }

  // test connection
  Message msg;
  std::string strmsg;

  msg.set_info(rank, srank, MessageType::REQ_ECHO, MessageKind::DEFAULT);
  msg.set_msg("Hello!");

  verboseStream << "ADNetClient sending handshake message to server" << std::endl;
  ZMQNet::send(m_socket, msg.data());

  msg.clear();

  verboseStream << "ADNetClient waiting for handshake response from server" << std::endl;
  try{
    ZMQNet::recv(m_socket, strmsg);
  }catch(const std::exception &err){
    recoverable_error("Connect error to parameter server, ADNetClient failed to initialize");
    return;
  }

  verboseStream << "ADNetClient handshake response received" << std::endl;

  msg.set_msg(strmsg, true);

  if (msg.buf().compare("Hello!I am NET!") != 0){
    recoverable_error("Connect error to parameter server: response message not as expected (ZMQNET)! Got:" + msg.buf());
    return;
  }

  m_use_ps = true;
  headProgressStream(rank) << "ADNetClient rank " << rank << " successfully connected to server " << sname << std::endl;
}

void ADZMQNetClient::disconnect_ps() {
  if (!m_use_ps) return;
  verboseStream << "ADNetClient rank " << m_rank << " disconnecting from PS" << std::endl;
  verboseStream << "ADNetClient rank " << m_rank << " sending disconnect message" << std::endl;
  Message msg;
  msg.set_info(0, 0, MessageType::REQ_QUIT, MessageKind::DEFAULT);
  msg.set_msg("");
  send_and_receive(msg);
  verboseStream << "ADNetClient rank " << m_rank << " disconnected from PS" << std::endl;
  m_use_ps = false;
}


std::string ADZMQNetClient::send_and_receive(const Message &msg){
  PerfTimer timer;
  std::string send_msg = msg.data(), recv_msg;
  //Send local parameters to PS
  ZMQNet::send(m_socket, send_msg);

  //Receive global parameters from PS
  ZMQNet::recv(m_socket, recv_msg);

#ifdef _PERF_METRIC
  if(m_perf){
    m_perf->add("net_client_send_recv_time_ms", timer.elapsed_ms());
    m_perf->add("net_client_send_bytes", send_msg.size()); //1 char = 1 byte
    m_perf->add("net_client_recv_bytes", recv_msg.size());
  }
#endif

  return recv_msg;
}

void ADZMQNetClient::stopServer(){
  verboseStream << "Client is sending stop request to server" << std::endl;
  Message msg;
  msg.set_info(0, 0, MessageType::REQ_QUIT, MessageKind::CMD);
  msg.set_msg("");
  send_and_receive(msg);
}

#endif








#ifdef _USE_MPINET

ADMPINetClient::~ADMPINetClient(){
  disconnect_ps();
}

void ADMPINetClient::connect_ps(int rank, int srank, std::string sname) {
  m_rank = rank;
  m_srank = srank;

  int rs;
  char port[MPI_MAX_PORT_NAME];

  rs = MPI_Lookup_name(sname.c_str(), MPI_INFO_NULL, port);
  if (rs != MPI_SUCCESS) return;

  rs = MPI_Comm_connect(port, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &m_comm);
  if (rs != MPI_SUCCESS) return;

  // test connection
  Message msg;
  msg.set_info(m_rank, m_srank, MessageType::REQ_ECHO, MessageKind::DEFAULT);
  msg.set_msg("Hello!");

  MPINet::send(m_comm, msg.data(), m_srank, MessageType::REQ_ECHO, msg.count());

  MPI_Status status;
  int count;
  MPI_Probe(m_srank, MessageType::REP_ECHO, m_comm, &status);
  MPI_Get_count(&status, MPI_BYTE, &count);

  msg.clear();
  msg.set_msg(
	      MPINet::recv(m_comm, status.MPI_SOURCE, status.MPI_TAG, count), true
	      );

  if (msg.data_buffer().compare("Hello!I am NET!") != 0)
    {
      std::cerr << "Connect error to parameter server (MPINET)!\n";
      exit(1);
    }
  m_use_ps = true;
  //std::cout << "rank: " << m_rank << ", " << msg.data_buffer() << std::endl;
}


void ADMPINetClient::disconnect_ps() {
  if (!m_use_ps) return;

  verboseStream << "ADNetClient rank " << m_rank << " disconnecting from PS" << std::endl;
  MPI_Barrier(MPI_COMM_WORLD);
  if (m_rank == 0)
    {
      Message msg;
      msg.set_info(m_rank, m_srank, MessageType::REQ_QUIT, MessageKind::CMD);
      msg.set_msg(MessageCmd::QUIT);
      MPINet::send(m_comm, msg.data(), m_srank, MessageType::REQ_QUIT, msg.count());
    }
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_disconnect(&m_comm);
  verboseStream << "ADNetClient rank " << m_rank << " disconnected from PS" << std::endl;
  m_use_ps = false;
}

std::string ADMPINetClient::send_and_receive(const Message &msg){  
  PerfTimer timer;
  std::string send_msg = msg.data(), recv_msg;
  MessageType req_type = MessageType(msg.type());
  MessageType rep_type;
  switch(req_type){
  case REQ_ADD:
    rep_type = REP_ADD; break;
  case REQ_GET:
    rep_type = REP_GET; break;
  case REQ_CMD:
    rep_type = REP_CMD; break;
  case REQ_QUIT:
    rep_type = REP_QUIT; break;
  case REQ_ECHO:
    rep_type = REP_ECHO; break;
  default:
    throw std::runtime_error("Invalid request type");
  }

  MPINet::send(m_comm, send_msg, m_srank, req_type, msg.count());

  MPI_Status status;
  int count;
  MPI_Probe(m_srank, rep_type, m_comm, &status);
  MPI_Get_count(&status, MPI_BYTE, &count);

  recv_msg = MPINet::recv(m_comm, status.MPI_SOURCE, status.MPI_TAG, count);

#ifdef _PERF_METRIC
  if(m_perf){
    m_perf->add("net_client_send_recv_time_ms", timer.elapsed_ms());
    m_perf->add("net_client_send_bytes", send_msg.size()); //1 char = 1 byte
    m_perf->add("net_client_recv_bytes", recv_msg.size());
  }
#endif

  return recv_msg;
}
  
#endif



void ADLocalNetClient::connect_ps(int rank, int srank, std::string sname){
  if(LocalNet::globalInstance() == nullptr) fatal_error("No LocalNet instance exists");
  m_use_ps = true;
}


void ADLocalNetClient::disconnect_ps(){
  m_use_ps = false;
}


std::string ADLocalNetClient::send_and_receive(const Message &msg){
  if(!m_use_ps) fatal_error("User should call connect_ps prior to sending/receiving messages");

  PerfTimer timer;    
  std::string send_msg = msg.data();
  std::string recv_msg = LocalNet::send_and_receive(send_msg);

#ifdef _PERF_METRIC
  if(m_perf){
    m_perf->add("net_client_send_recv_time_ms", timer.elapsed_ms());
    m_perf->add("net_client_send_bytes", send_msg.size()); //1 char = 1 byte
    m_perf->add("net_client_recv_bytes", recv_msg.size());
  }
#endif

  return recv_msg;
}


 
struct ClientActionConnect: public ADThreadNetClient::ClientAction{
  int rank;
  int srank;
  std::string sname;

  ClientActionConnect(int rank, int srank, const std::string &sname): rank(rank), srank(srank), sname(sname){}

  void perform(ADNetClient &client){
    std::cout << "Connecting to client" << std::endl;
    client.connect_ps(rank, srank, sname);
  }
  bool do_delete() const{ return true; }
};

struct ClientActionDisconnect: public ADThreadNetClient::ClientAction{
  void perform(ADNetClient &client){
    std::cout << "Disconnecting from client" << std::endl;
    client.disconnect_ps();
  }
  bool do_delete() const{ return true; }
  bool shutdown_worker() const{ return true; }
};

//Make the worker wait for some time, for testing
struct ClientActionWait: public ADThreadNetClient::ClientAction{
  size_t wait_ms;
  ClientActionWait(size_t wait_ms): wait_ms(wait_ms){}

  void perform(ADNetClient &client){
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
  }
  bool do_delete() const{ return true; }
};

struct ClientActionBlockingSendReceive: public ADThreadNetClient::ClientAction{
  std::mutex m;
  std::condition_variable cv;
  std::string *recv;
  Message const *send;  //it's blocking so we know that the object will live long enough
  bool complete;

  ClientActionBlockingSendReceive(std::string *recv, Message const *send): send(send), recv(recv), complete(false){}

  void perform(ADNetClient &client){
    *recv = client.send_and_receive(*send);

    {
      std::unique_lock<std::mutex> lk(m);
      complete = true;
      cv.notify_one();
    }
  }
  bool do_delete() const{ return false; }

  void wait_for(){
    std::unique_lock<std::mutex> l(m);
    cv.wait(l, [&]{ return complete; });
  }
};

//Return message is just dumped
struct ClientActionAsyncSend: public ADThreadNetClient::ClientAction{
  Message send;  //copy of send message because we don't know how long it will be before it sends

  ClientActionAsyncSend(const Message &send): send(send){}

  void perform(ADNetClient &client){
    Message recv;
    client.send_and_receive(recv, send);
  }
  bool do_delete() const{ return true; }
};

struct ClientActionSetRecvTimeout: public ADThreadNetClient::ClientAction{
  int timeout;
  ClientActionSetRecvTimeout(const int timeout): timeout(timeout){}

  void perform(ADNetClient &client){
    client.setRecvTimeout(timeout);
  }

  bool do_delete() const{return true;}
};



size_t ADThreadNetClient::getNwork() const{
  std::lock_guard<std::mutex> l(m);
  return queue.size();
}
ADThreadNetClient::ClientAction* ADThreadNetClient::getWorkItem(){
  std::lock_guard<std::mutex> l(m);
  ClientAction *work_item = queue.front();
  queue.pop();
  return work_item;
}
    
/**
 * @brief Create the worker thread
 * @param local Use a local (in process) communicator if true, otherwise use the default network communicator
 */
void ADThreadNetClient::run(bool local){
  worker = std::thread([&,local](){
      ADNetClient *client = nullptr;
      if(local){
	client = new ADLocalNetClient;
      }else{
#ifdef _USE_MPINET
	client = new ADMPINetClient;
#else
	client = new ADZMQNetClient;
#endif
      }
      bool shutdown = false;

      while(!shutdown){
	size_t nwork = getNwork();
	while(nwork > 0){
	  ClientAction* work_item = getWorkItem();
	  work_item->perform(*client);
	  shutdown = shutdown || work_item->shutdown_worker();

	  if(work_item->do_delete()) delete work_item;
	  nwork = getNwork();
	}
	if(shutdown){
	  if(nwork > 0) fatal_error("Worker was shut down before emptying its queue!");
	}else{
	  std::this_thread::sleep_for(std::chrono::milliseconds(80));
	}  
      }
      delete client;
    });
}

ADThreadNetClient::ADThreadNetClient(bool local){
  run(local);
}
    
void ADThreadNetClient::enqueue_action(ClientAction *action){
  std::lock_guard<std::mutex> l(m);
  queue.push(action);
}

void ADThreadNetClient::connect_ps(int rank, int srank, std::string sname){
  m_rank = rank;
  m_srank = srank;
  enqueue_action(new ClientActionConnect(rank, srank,sname));
  m_use_ps = true;
}
void ADThreadNetClient::disconnect_ps(){
  enqueue_action(new ClientActionDisconnect());
}
std::string ADThreadNetClient::send_and_receive(const Message &send){
  std::string recv;
  ClientActionBlockingSendReceive action(&recv, &send);
  enqueue_action(&action);
  action.wait_for();
  return recv;
}
void ADThreadNetClient::async_send(const Message &send){
  enqueue_action(new ClientActionAsyncSend(send));
}

void ADThreadNetClient::setRecvTimeout(const int timeout_ms) {
  enqueue_action(new ClientActionSetRecvTimeout(timeout_ms));
}    

ADThreadNetClient::~ADThreadNetClient(){
  disconnect_ps();
  worker.join();
}

