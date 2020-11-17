#pragma once

#ifdef _USE_MPINET
#include "chimbuko/net/mpi_net.hpp"
#else
#include "chimbuko/net/zmq_net.hpp"
#endif
#include "chimbuko/message.hpp"
#include "chimbuko/util/PerfStats.hpp"

namespace chimbuko{


  /**
   * @brief A wrapper class to facilitate communications between the AD and the parameter server
   */
  class ADNetClient{
  public:
    ADNetClient();

    ~ADNetClient();
    
    /**
     * @brief check if the parameter server is in use
     * 
     * @return true if the parameter server is in use
     * @return false if the parameter server is not in use
     */
    bool use_ps() const { return m_use_ps; }

    /**
     * @brief connect to the parameter server
     * 
     * @param rank this process rank
     * @param srank server process rank. If using ZMQnet this is not applicable
     * @param sname server name. If using ZMQNet this is the server ip address, for MPINet it is not applicable
     */
    void connect_ps(int rank, int srank = 0, std::string sname="MPINET");
    /**
     * @brief disconnect from the connected parameter server
     * 
     * Called automatically by destructor if not previously called
     */
    void disconnect_ps();

    /**
     * @brief Return the MPI rank of the parameter server
     */
    int get_server_rank() const{ return m_srank; }

    /**
     * @brief Return the MPI rank of this client
     */
    int get_client_rank() const{ return m_rank; }

    /**
     * @brief Send a message to the parameter server and receive the response in a serialized format
     * @param msg The message
     * @return The response message in serialized format. Use Message::set_msg( <serialized_msg>,  true )  to unpack
     */       
    std::string send_and_receive(const Message &msg);

    /**
     * @brief Send a message to the parameter server and receive the response both as Message objects
     * @param send The sent message
     * @param recv The received message
     *
     * Note recv and send can be the same object
     */       
    void send_and_receive(Message &recv, const Message &send);

    /**
     * @brief If linked timing and packet size information will be gathered
     */
    void linkPerf(PerfStats* perf){ m_perf = perf; }

#ifdef _USE_ZMQNET
    /**
     * @brief Set the timeout on blocking receives. Must be called prior to connecting
     */
    void setRecvTimeout(const int timeout_ms){ m_recv_timeout_ms = timeout_ms; }

    /**
     * @brief Get the zeroMQ socket
     */
    void * getZMQsocket(){ return m_socket; }

    /**
     * @brief Get the zeroMQ context
     */
    void * getZMQcontext(){ return m_context; }
    
#endif


  private:
    bool m_use_ps;                           /**< true if the parameter server is in use */
    int m_rank;                              /**< MPI rank of current process */
    int m_srank;                             /**< server process rank                    */
#ifdef _USE_MPINET
    MPI_Comm m_comm;                         /**< Instance of the MPI communicator */
#else
    void* m_context;                         /**< ZeroMQ context */
    void* m_socket;                          /**< ZeroMQ socket */
    int m_recv_timeout_ms;                   /**< Timeout (in ms) on blocking receives (default 30s)*/
#endif
    PerfStats * m_perf;                      /**< Performance monitoring */
  };

  



};
