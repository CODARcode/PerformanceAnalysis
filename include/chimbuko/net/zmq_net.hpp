#pragma once
#ifdef _USE_ZMQNET

#include "chimbuko/net.hpp"
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
    void finalize() override;
#ifdef _PERF_METRIC
    void run(std::string logdir="./") override;
#else
    void run() override;
#endif
    void stop() override;
    std::string name() const override { return "ZMQNET"; }

    static int send(void* socket, const std::string& strmsg);
    static int recv(void* socket, std::string& strmsg);

  protected:
    void init_thread_pool(int nt) override;

  private:
    /**
       @brief Route a message to/from worker thread pool
    */
    bool recvAndSend(void* skFrom, void* skTo);

  private:
    void* m_context; /** ZeroMQ context pointer */
    long long m_n_requests;
    std::vector<std::thread> m_threads; /** The pool of thread workers */
  };

} // end of chimbuko namespace
#endif // CHIMBUKO_USE_ZMQNET
