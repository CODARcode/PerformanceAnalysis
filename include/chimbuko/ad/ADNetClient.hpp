#pragma once

#ifdef _USE_MPINET
#include "chimbuko/net/mpi_net.hpp"
#else
#include "chimbuko/net/zmq_net.hpp"
#endif
#include "chimbuko/message.hpp"

namespace chimbuko{


  /**
   * @brief A wrapper class to facilitate communications between the AD and the parameter server
   */
  class ADNetClient{
  public:
    ADNetClient();
    
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
     * @brief Send a message to the parameter server and receive the response
     * @param msg The message
     * @return The response message in string format. This is a JSON object with 'Header' and 'Buffer' fields
     */       
    std::string send_and_receive(const Message &msg);
    
  private:
    bool m_use_ps;                           /**< true if the parameter server is in use */
    int m_rank;
    int m_srank;                             /**< server process rank                    */
#ifdef _USE_MPINET
    MPI_Comm m_comm;
#else
    void* m_context;                         /**< ZeroMQ context */
    void* m_socket;                          /**< ZeroMQ socket */
#endif
  };

  



};
