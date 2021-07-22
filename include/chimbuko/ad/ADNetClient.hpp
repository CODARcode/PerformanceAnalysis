#pragma once
#include <chimbuko_config.h>
#ifdef _USE_MPINET
#include "chimbuko/net/mpi_net.hpp"
#else
#include "chimbuko/net/zmq_net.hpp"
#endif
#include "chimbuko/net/local_net.hpp"
#include "chimbuko/message.hpp"
#include "chimbuko/util/PerfStats.hpp"
#include "chimbuko/util/string.hpp"
#include "chimbuko/util/error.hpp"
#include "chimbuko/util/time.hpp"
 
namespace chimbuko{


  /**
   * @brief A wrapper class to facilitate communications between the AD and the parameter server
   */
  class ADNetClient{
  public:
    ADNetClient();

    virtual ~ADNetClient();
    
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
    virtual void connect_ps(int rank, int srank = 0, std::string sname="MPINET") = 0;
    /**
     * @brief disconnect from the connected parameter server
     * 
     * Called automatically by destructor if not previously called
     */
    virtual void disconnect_ps() = 0;

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
    virtual std::string send_and_receive(const Message &msg) const = 0;

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

    /**
     * @brief Set the timeout for receiving messages (implementation dependent)
     */
    virtual void setRecvTimeout(const int timeout_ms){ }

  protected:
    bool m_use_ps;                           /**< true if the parameter server is in use */
    int m_rank;                              /**< MPI rank of current process */
    int m_srank;                             /**< server process rank                    */
    PerfStats * m_perf;                      /**< Performance monitoring */
  };

  
#ifdef _USE_ZMQNET
  /**
   * @brief Implementation of the ADNetClient interface for the ZMQNet network
   */
  class ADZMQNetClient: public ADNetClient{
  public:

    ADZMQNetClient();

    ~ADZMQNetClient();   

    /**
     * @brief connect to the parameter server
     * 
     * @param rank this process rank
     * @param srank Ignored for this class
     * @param sname The server ip address
     */
    void connect_ps(int rank, int srank = 0, std::string sname="MPINET") override;
    /**
     * @brief disconnect from the connected parameter server
     * 
     * Called automatically by destructor if not previously called
     */
    void disconnect_ps() override;

    using ADNetClient::send_and_receive;

    /**
     * @brief Send a message to the parameter server and receive the response in a serialized format
     * @param msg The message
     * @return The response message in serialized format. Use Message::set_msg( <serialized_msg>,  true )  to unpack
     */       
    std::string send_and_receive(const Message &msg) const override;


    /**
     * @brief Set the timeout on blocking receives. Must be called prior to connecting
     */
    void setRecvTimeout(const int timeout_ms) override{ m_recv_timeout_ms = timeout_ms; }

    /**
     * @brief Get the zeroMQ socket
     */
    void * getZMQsocket(){ return m_socket; }

    /**
     * @brief Get the zeroMQ context
     */
    void * getZMQcontext(){ return m_context; }
    
    /**
     * @brief Issue a stop command to the server. The server will then stop once all clients have disconnected and all messages processed
     */
    void stopServer() const;

  private:
    void* m_context;                         /**< ZeroMQ context */
    void* m_socket;                          /**< ZeroMQ socket */
    int m_recv_timeout_ms;                   /**< Timeout (in ms) on blocking receives (default 30s)*/
  };
#endif

#ifdef _USE_MPINET
  /**
   * @brief Implementation of the ADNetClient interface for the MPINet network
   */
  class ADMPINetClient: public ADNetClient{
  public:
    ~ADMPINetClient();   

    /**
     * @brief connect to the parameter server
     * 
     * @param rank this process rank
     * @param srank server process rank
     * @param sname Ignored for this class
     */
    void connect_ps(int rank, int srank = 0, std::string sname="MPINET") override;
    /**
     * @brief disconnect from the connected parameter server
     * 
     * Called automatically by destructor if not previously called
     */
    void disconnect_ps() override;

    using ADNetClient::send_and_receive;

    /**
     * @brief Send a message to the parameter server and receive the response in a serialized format
     * @param msg The message
     * @return The response message in serialized format. Use Message::set_msg( <serialized_msg>,  true )  to unpack
     */       
    std::string send_and_receive(const Message &msg) const override;

  
  private:
    MPI_Comm m_comm;                         /**< Instance of the MPI communicator */
  };
  
#endif


 
  /**
   * @brief Implementation of ADNetClient for intraprocess communications
   */
  class ADLocalNetClient: public ADNetClient{
  public:
    /**
     * @brief connect to the parameter server
     * 
     * @param rank Ignored
     * @param srank Ignored
     * @param sname Ignored
     */
    void connect_ps(int rank, int srank = 0, std::string sname="MPINET") override;
    /**
     * @brief disconnect from the connected parameter server
     * 
     * Called automatically by destructor if not previously called
     */
    void disconnect_ps() override;

    using ADNetClient::send_and_receive;
    
    /**
     * @brief Send a message to the parameter server and receive the response in a serialized format
     * @param msg The message
     * @return The response message in serialized format. Use Message::set_msg( <serialized_msg>,  true )  to unpack
     */       
    std::string send_and_receive(const Message &msg) const override;
  };


  /**
   * @brief ADNetClient inside a worker thread with blocking send/receive and non-blocking send
   */
  class ADThreadNetClient{
  public:
    /**
     * @brief Virtual class representing actions performed by the worker thread
     */
    struct ClientAction{
      /** 
       * @brief Perform the action utilizing the underlying net implementation
       */
      virtual void perform(ADNetClient &client) = 0;
      
      /**
       * @brief Whether to delete the work object (instance of ClientAction) after completion
       */
      virtual bool do_delete() const = 0;
      
      /**
       * @brief Whether to shutdown the worker thread after completing the action
       */
      virtual bool shutdown_worker() const{ return false; }

      virtual ~ClientAction(){}
    };

  private:
    std::thread worker;
    mutable std::mutex m;
    std::queue<ClientAction*> queue; /**< The queue of net operations*/

    int m_rank;
    int m_srank;
    bool m_use_ps;
    PerfStats * m_perf;
    
    /**
     * @brief Get the number of outstanding net operations
     */
    size_t getNwork() const;
    /**
     * @brief Get the next net operation
     */
    ClientAction* getWorkItem();
    
    /**
     * @brief Create the worker thread
     * @param local Use a local (in process) communicator if true, otherwise use the default network communicator
     */
    void run(bool local = false);

  public:
    /**
     * @brief Constructor
     * @param local Use a local (in process) communicator if true, otherwise use the default network communicator
     */
    ADThreadNetClient(bool local = false);

    /**
     * @brief Add an action to the queue
     *
     *Use only if you know what you are doing!
     */
    void enqueue_action(ClientAction *action);

    /**
     * @brief Connect to the parameter server
     */
    void connect_ps(int rank, int srank = 0, std::string sname="MPINET");

    /**
     * @brief Disconnect from the parameter server
     */
    void disconnect_ps();

    /**
     * @brief Perform a blocking send and receive operation
     */
    void send_and_receive(Message &recv, const Message &send);

    /**
     * @brief Perform a non-blocking send operation
     */
    void async_send(const Message &send);

    /**
     * @brief Is the parameter server in use / connected?
     */
    bool use_ps() const { return m_use_ps; }

    /**
     * @brief Link a performance monitoring instance
     */
    void linkPerf(PerfStats* perf){ m_perf = perf; }

    /**
     * @brief Get the MPI rank of the server (MPINet)
     */
    int get_server_rank() const{ return m_srank; }
    
    /**
     * @brief Get the MPI rank of the client
     */
    int get_client_rank() const{ return m_rank; }

    /**
     * @brief Set a timeout (in ms) on receiving a response message
     */
    void setRecvTimeout(const int timeout_ms);

    ~ADThreadNetClient();
  };

 

};
