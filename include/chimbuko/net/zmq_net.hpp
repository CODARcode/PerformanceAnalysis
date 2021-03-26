#pragma once
#ifdef _USE_ZMQNET

#include <chimbuko/net.hpp>
#include <chimbuko/util/PerfStats.hpp>
#include <zmq.h>
#include <vector>
#include <thread>

namespace chimbuko {
  
  /**
     @brief A network interface using ZeroMQ
  */
  class ZMQNet : public NetInterface {
  public:

    enum class Status { NotStarted, StartingUp, Running, ShuttingDown, StoppedByRequest, StoppedBySignal, StoppedByTimeOut, StoppedByError, StoppedAutomatically };

    ZMQNet();
    ~ZMQNet();

    /**
     * @brief (virtual) initialize network interface
     * 
     * @param argc command line argc 
     * @param argv command line argv
     * @param nt the number of threads for a thread pool
     */
    void init(int* argc, char*** argv, int nt) override;

    /**
     * @brief Finalize network
     */
    void finalize() override;
#ifdef _PERF_METRIC
    /**
     * @brief Run network server with performance logging to provided directory
     */
    void run(std::string logdir="./") override;
#else
    /**
     * @brief (virtual) Run network server
     */    
    void run() override;
#endif
    /**
     * @brief Stop network server
     * 
     */
    void stop() override;


    /**
     * @brief Name of network server
     * @return std::string name of network server
     */
    std::string name() const override { return "ZMQNET"; }

    /**
     * @brief Send the data packet to the server on the provided socket
     */
    static int send(void* socket, const std::string& strmsg);

    /**
     * @brief Receive the data packet to the server from the provided socket
     */
    static int recv(void* socket, std::string& strmsg);

    /**
     * @brief Set the maximum number of messages that the router thread will route front->back and back->front per poll cycle
     */
    void setMaxMsgPerPollCycle(const int max_msg){ m_max_pollcyc_msg = max_msg; }

    /**
     * @brief Set the number of IO threads used by ZeroMQ (default 1). Must be called prior to init(...)
     */
    void setIOthreads(const int nt){ m_io_threads = nt; }

    /**
     * @brief Set the port upon which the connection is made. Must be called prior to run(..). Default 5559
     */
    void setPort(const int port){ m_port = port; }

    /**
     * @brief Set the rule for automatic shutdown once all clients have disconnected (default true)
     */
    void setAutoShutdown(const bool to){ m_autoshutdown = to; }


    /**
     * @brief Set the timeout on polling for client requests or responses from worker threads (-1 = no timeout [default])
     */
    void setTimeOut(const long time_ms){ m_poll_timeout = time_ms; }

    /**
     * @brief Query the status of the network endpoint (presumably from another thread)
     */
    Status getStatus() const{ return m_status; }

  protected:
    /**
     * @brief initialize thread pool 
     * 
     * @param nt the number threads in the pool
     */
    void init_thread_pool(int nt) override;

  private:
    /**
     * @brief Route a message to/from worker thread pool
     *
     * @param skFrom ZMQ origin socket 
     * @param skTo ZMQ destination socket
     * @param max_msg The maximum number of messages this function will attempt to drain from the queue (including disconnect message)
     * @return the number of messages routed
     */
    int recvAndSend(void* skFrom, void* skTo, int max_msg);

  private:
    void* m_context; /**< ZeroMQ context pointer */
    long long m_n_requests; /**< Accumulated number of RPC requests */
    std::vector<std::thread> m_threads; /**< The pool of thread workers */
    PerfStats m_perf; /**< Performance monitoring */
    std::vector<PerfStats> m_perf_thr; /**< Performance monitoring for worker threads; will be combined with m_perf before write*/
    int m_max_pollcyc_msg; /**< Maximum number of front->back and back->front messages that will be routed per poll cycle. Too many and we risk starving a socket, too few and might hit perf issues*/
    int m_io_threads; /**< Set the number of IO threads used by ZeroMQ (default 1)*/
    mutable std::mutex m_mutex;
    int m_clients; /**< Number of connected clients*/
    bool m_client_has_connected; /**< At least one client has connected previously*/
    int m_port; /**< The port upon which the net connects*/
    bool m_autoshutdown; /**< The network will shutdown once all clients have disconnected*/
    Status m_status; /**< Monitoring of status */
    long m_poll_timeout; /**< The timeout (in ms) after which on no activity the network with shutdown (default -1: infinite)*/
  };

} // end of chimbuko namespace
#endif // CHIMBUKO_USE_ZMQNET
