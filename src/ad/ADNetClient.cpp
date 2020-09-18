#include <chimbuko/ad/ADNetClient.hpp>
#include <chimbuko/verbose.hpp>
#include <mpi.h>

using namespace chimbuko;

ADNetClient::ADNetClient() 
  : m_use_ps(false), m_perf(nullptr)
{
#ifdef _USE_ZMQNET
    m_context = nullptr;
    m_socket = nullptr;
#endif
}

ADNetClient::~ADNetClient(){
  disconnect_ps();
}

void ADNetClient::connect_ps(int rank, int srank, std::string sname) {
    m_rank = rank;
    m_srank = srank;

#ifdef _USE_MPINET
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
#else
    m_context = zmq_ctx_new();
    m_socket = zmq_socket(m_context, ZMQ_REQ);
    if(zmq_connect(m_socket, sname.c_str()) == -1){
      std::string err = strerror(errno);      
      throw std::runtime_error("ZMQ failed to connect to socket at address " + sname + ", due to error: " + err);
    }

    // test connection
    Message msg;
    std::string strmsg;

    msg.set_info(rank, srank, MessageType::REQ_ECHO, MessageKind::DEFAULT);
    msg.set_msg("Hello!");

    VERBOSE(std::cout << "ADNetClient sending handshake message to server" << std::endl);
    ZMQNet::send(m_socket, msg.data());

    msg.clear();

    VERBOSE(std::cout << "ADNetClient waiting for handshake response from server" << std::endl);
    ZMQNet::recv(m_socket, strmsg);
    VERBOSE(std::cout << "ADNetClient handshake response received" << std::endl);
    
    msg.set_msg(strmsg, true);

    if (msg.buf().compare("Hello!I am NET!") != 0)
    {
      throw std::runtime_error("Connect error to parameter server: response message not as expected (ZMQNET)! Got:" + msg.buf());
    } 
    m_use_ps = true;
    std::cout << "ADNetClient rank " << rank << " successfully connected to server " << sname << std::endl;
#endif
    //MPI_Barrier(MPI_COMM_WORLD);
}

void ADNetClient::disconnect_ps() {
    if (!m_use_ps) return;

    VERBOSE(std::cout << "ADNetClient rank " << m_rank << " disconnecting from PS" << std::endl);
#ifdef _USE_MPINET
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
#else
    VERBOSE(std::cout << "ADNetClient rank " << m_rank << " sending disconnect message" << std::endl);
    Message msg;
    msg.set_info(0, 0, MessageType::REQ_QUIT, MessageKind::DEFAULT);
    msg.set_msg("");
    send_and_receive(msg);
#endif
    VERBOSE(std::cout << "ADNetClient rank " << m_rank << " disconnected from PS" << std::endl);
    m_use_ps = false;
}

std::string ADNetClient::send_and_receive(const Message &msg){  
  PerfTimer timer;
  std::string send_msg = msg.data(), recv_msg;
#ifdef _USE_MPINET
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
#else
  //Send local parameters to PS
  ZMQNet::send(m_socket, send_msg);
  
  //Receive global parameters from PS
  ZMQNet::recv(m_socket, recv_msg);   
#endif

#ifdef _PERF_METRIC
  if(m_perf){
    m_perf->add("net_client_send_recv_time_ms", timer.elapsed_ms());
    m_perf->add("net_client_send_bytes", send_msg.size()); //1 char = 1 byte
    m_perf->add("net_client_recv_bytes", recv_msg.size());
  }
#endif  

  return recv_msg;
}
