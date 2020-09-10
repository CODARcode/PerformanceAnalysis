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
     * @return first (int) element is the number of messages routed (i.e. excluding disconnect message which are not routed),
     *         second (bool) element indicates if a disconnect message was received
     */
    std::pair<int,bool> recvAndSend(void* skFrom, void* skTo, int max_msg);

  private:
    void* m_context; /**< ZeroMQ context pointer */
    long long m_n_requests; /**< Accumulated number of RPC requests */
    std::vector<std::thread> m_threads; /**< The pool of thread workers */
    PerfStats m_perf; /**< Performance monitoring */
    std::vector<PerfStats> m_perf_thr; /**< Performance monitoring for worker threads; will be combined with m_perf before write*/
  };

} // end of chimbuko namespace
#endif // CHIMBUKO_USE_ZMQNET
