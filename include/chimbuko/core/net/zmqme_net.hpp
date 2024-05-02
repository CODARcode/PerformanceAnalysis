#pragma once
#include<chimbuko_config.h>
#ifdef _USE_ZMQNET

#include <chimbuko/core/net.hpp>
#include <chimbuko/util/PerfStats.hpp>
#include <zmq.h>
#include <vector>
#include <thread>
#include <atomic>

namespace chimbuko {
  
  /**
     @brief A multi-endpoint, multi-threaded interface using ZeroMQ
  */
  class ZMQMENet : public NetInterface {
  public:
    ZMQMENet();
    ~ZMQMENet();

    /**
     * @brief (virtual) initialize network interface
     * 
     * @param argc command line argc 
     * @param argv command line argv
     * @param nt the number of endpoint threads
     */
    void init(int* argc, char*** argv, int nt) override;

    /**
     * @brief Finalize network; blocking wait for worker threads to finish
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
    std::string name() const override { return "ZMQMENET"; }

    /**
     * @brief Receive the data packet to the server from the provided socket with a non-blocking receive
     * @return -1 if no messages on the queue, otherwise the byte size of the message
     */
    static int recv_NB(void* socket, std::string& strmsg);


    /**
     * @brief Set the base port upon which the connection is made (thread ports are offset by thread index). Must be called prior to run(..). Default 5559
     */
    void setBasePort(const int port){ m_base_port = port; }

  protected:

    /**
     * @brief initialize thread pool 
     * 
     * @param nt the number threads in the pool
     */
    void init_thread_pool(int nt) override;

  private:
    int m_base_port; /**< Port of first endpoint */
    int m_nthread; /**< Number of endpoint threads */
    std::vector<std::thread> m_threads; /**< The pool of thread workers */
    PerfStats m_perf; /**< Performance monitoring */
    std::vector<PerfStats> m_perf_thr; /**< Performance monitoring for worker threads; will be combined with m_perf before write*/
    std::vector<int> m_clients_thr; /**< Tracker of number of connected clients, used to determine when a thread exits*/
    std::vector<int> m_client_has_connected; /**< Has a client previously connected to this worker?*/
    bool m_finalized; /**< Has previously been finalized */
  };

} // end of chimbuko namespace
#endif // CHIMBUKO_USE_ZMQNET
