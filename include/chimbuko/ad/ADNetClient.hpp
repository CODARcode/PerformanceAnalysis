#pragma once

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

  //Actions performed by the worker thread
  struct ClientAction{
    virtual void perform(ADNetClient &client) = 0;
    virtual bool do_delete() const = 0; //whether to delete the work object after completion
    virtual bool shutdown_worker() const{ return false; } //whether to shutdown the worker after completing the action
    virtual ~ClientAction(){}
  };
  
  struct ClientActionConnect: public ClientAction{
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

  struct ClientActionDisconnect: public ClientAction{
    void perform(ADNetClient &client){
      std::cout << "Disconnecting from client" << std::endl;
      client.disconnect_ps();
    }
    bool do_delete() const{ return true; }
    bool shutdown_worker() const{ return true; }
  };

  //Make the worker wait for some time, for testing
  struct ClientActionWait: public ClientAction{
    size_t wait_ms;
    ClientActionWait(size_t wait_ms): wait_ms(wait_ms){}

    void perform(ADNetClient &client){
      std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
    }
    bool do_delete() const{ return true; }
  };

  struct ClientActionBlockingSendReceive: public ClientAction{
    std::mutex m;
    std::condition_variable cv;
    Message *recv;
    Message const *send;  //it's blocking so we know that the object will live long enough
    bool complete;

    ClientActionBlockingSendReceive(Message *recv, Message const *send): send(send), recv(recv), complete(false){}

    void perform(ADNetClient &client){
      client.send_and_receive(*recv, *send);

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
  struct ClientActionAsyncSend: public ClientAction{
    Message send;  //copy of send message because we don't know how long it will be before it sends

    ClientActionAsyncSend(const Message &send): send(send){}

    void perform(ADNetClient &client){
      Message recv;
      client.send_and_receive(recv, send);
    }
    bool do_delete() const{ return true; }
  };

  struct ClientActionSetRecvTimeout: public ClientAction{
    int timeout;
    ClientActionSetRecvTimeout(const int timeout): timeout(timeout){}

    void perform(ADNetClient &client){
      client.setRecvTimeout(timeout);
    }

    bool do_delete() const{return true;}
  };

  //ADNetClient inside a worker thread with blocking send/receive and non-blocking send
  class ADThreadNetClient{
    std::thread worker;
    mutable std::mutex m;
    std::queue<ClientAction*> queue;

    size_t getNwork() const{
      std::lock_guard<std::mutex> l(m);
      return queue.size();
    }
    ClientAction* getWorkItem(){
      std::lock_guard<std::mutex> l(m);
      ClientAction *work_item = queue.front();
      queue.pop();
      return work_item;
    }
    
    /**
     * @brief Create the worker thread
     * @param local Use a local (in process) communicator if true, otherwise use the default network communicator
     */
    void run(bool local = false){
      worker = std::thread([&](){
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

  public:
    /**
     * @brief Constructor
     * @param local Use a local (in process) communicator if true, otherwise use the default network communicator
     */
    ADThreadNetClient(bool local = false){
      run(local);
    }
    
    //Use only if you know what you are doing!
    void enqueue_action(ClientAction *action){
      std::lock_guard<std::mutex> l(m);
      queue.push(action);
    }

    void connect_ps(int rank, int srank = 0, std::string sname="MPINET"){
      m_rank = rank;
      m_srank = srank;
      enqueue_action(new ClientActionConnect(rank, srank,sname));
      m_use_ps = true;
    }
    void disconnect_ps(){
        enqueue_action(new ClientActionDisconnect());
    }
    void send_and_receive(Message &recv, const Message &send){
      ClientActionBlockingSendReceive action(&recv, &send);
      enqueue_action(&action);
      action.wait_for();
    }
    void async_send(const Message &send){
      enqueue_action(new ClientActionAsyncSend(send));
    }

    bool use_ps() const { return m_use_ps; }

    void linkPerf(PerfStats* perf){ m_perf = perf; }

    int get_server_rank() const{ return m_srank; }
    
    int get_client_rank() const{ return m_rank; }

    void setRecvTimeout(const int timeout_ms) {
      enqueue_action(new ClientActionSetRecvTimeout(timeout_ms));
    }    

    ~ADThreadNetClient(){
      disconnect_ps();
      worker.join();
    }
   
   private:
    int m_rank;
    int m_srank;
    bool m_use_ps;
    PerfStats * m_perf;
  };

 

};
