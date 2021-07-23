#include "chimbuko/net/mpi_net.hpp"
#ifdef _USE_MPINET
#include "chimbuko/message.hpp"
#include "chimbuko/param/sstd_param.hpp"
// todo: replace with log
#include <iostream> 
#include <chrono>
#include <mpi.h>
using namespace chimbuko;

MPINet::MPINet() : m_inited(0), m_tpool(nullptr), m_stop(false)
{
  add_payload(new NetPayloadHandShake());
}

MPINet::~MPINet()
{
    if (m_tpool)
    {
        delete m_tpool;
    }    
}

void MPINet::init(int* argc, char*** argv, int nt){    
  //Check worker_idx = 0 for all payloads
  for(auto const &e : m_payloads)
    if(e.first != 0) throw std::runtime_error("MPINet all payloads must have worker_idx=0");
  

  // initialize MPI, if it didn't
  MPI_Initialized(&m_inited);
  if (!m_inited) 
    {
      MPI_Init_thread(argc, &(*argv), MPI_THREAD_MULTIPLE, &m_thread_provided);
    }

  // check thread level
  MPI_Query_thread(&m_thread_provided);
  if (m_thread_provided != MPI_THREAD_MULTIPLE)
    {
      std::cerr << "Currenlty only support MPI_THREAD_MULTIPLE mode\n";
      exit(1);
    }

  MPI_Comm_size(MPI_COMM_WORLD, &m_size);
  MPI_Comm_rank(MPI_COMM_WORLD, &m_rank);
  MPI_Barrier(MPI_COMM_WORLD);
  if (m_size > 1)
    {
      std::cerr << "Currently MPINET must be size of 1!\n";
      exit(1);
    }
    
  // initialize thread pool
  init_thread_pool(nt);
}

void MPINet::finalize()
{
    m_inited = 0;
    MPI_Finalize();
}

void MPINet::run()
{
    MPI_Info info;
    MPI_Comm client;
    MPI_Status status;
    // int flag;
    char port_name[MPI_MAX_PORT_NAME];
    int count;

    MPI_Open_port(MPI_INFO_NULL, port_name);
    MPI_Info_create(&info);
    MPI_Info_set(info, "ompi_global_scope", "true");
    MPI_Publish_name(name().c_str(), info, port_name);
    //std::cout << "[SERVER] port name: " << port_name << std::endl;
    // single connection only!!
    MPI_Comm_accept(port_name, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &client);

    while (!m_stop)
    {
        // blocking probe whether message comes
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, client, &status);
        MPI_Get_count(&status, MPI_BYTE, &count);
	    //std::cout << "receive request\n";
        int source = status.MPI_SOURCE;
        int tag = status.MPI_TAG;
        
        // stop server
        if (tag == MessageType::REQ_QUIT) {
            this->recv(client, source, tag, count);
            stop();
            MPI_Comm_disconnect(&client);
            MPI_Unpublish_name(name().c_str(), MPI_INFO_NULL, port_name);
            MPI_Close_port(port_name);
            continue;
        }

        // submit job
        m_tpool->sumit([&client, &m_payloads, this](int _source, int _tag, int _count){
            Message _msg, _msg_reply;
            std::string _recv_buffer; 
            
            _recv_buffer = this->recv(client, _source, _tag, _count);
            if (_recv_buffer.size()) {
                _msg.set_msg(_recv_buffer, true);
            }

            _msg_reply = _msg.createReply();
	    
	    auto kit = m_payloads.begin()->second.find(_msg.kind());
	    if(kit == m_payloads.begin()->second.end()) throw std::runtime_error("No payload associated with the message kind provided");
	    auto pit = kit->second.find(_msg.type());
	    if(pit == kit->second.end()) throw std::runtime_error("No payload associated with the message type provided");
	    
	    //Apply the payload
	    pit->second->action(_msg_reply, _msg);
            
	        //std::cout << "reply request\n";
            this->send(client, 
                _msg_reply.data(), 
                _msg_reply.dst(), 
                _msg_reply.type(), 
                _msg_reply.count()
            );

        }, source, tag, count);
    }
}

void MPINet::stop()
{
    m_stop = true;
    while (m_tpool->queue_size()) 
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    delete m_tpool;
    m_tpool = nullptr;
}

void MPINet::init_thread_pool(int nt)
{
    if (m_tpool == nullptr && nt > 0)
    {
        m_nt = nt;
        m_tpool = new threadPool(nt);
    }
}

std::string MPINet::recv(MPI_Comm& comm, int src, int tag, int count) 
{
    MPI_Status status;

    if (count == MPI_UNDEFINED || count == 0) {
        return "";
    }

    std::string recv_buffer;
    recv_buffer.resize(count);
    MPI_Recv((void*)&recv_buffer[0], count, MPI_BYTE, src, tag, comm, &status);

    return recv_buffer;
}

void MPINet::send(MPI_Comm& comm, const std::string& buf, int dest, int tag, int count)
{
    MPI_Send(buf.c_str(), count, MPI_BYTE, dest, tag, comm);
}

#endif // CHIMBUKO_USE_MPINET
