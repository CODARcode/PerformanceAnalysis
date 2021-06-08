#pragma once

#include <chimbuko/net.hpp>
#include <chimbuko/util/PerfStats.hpp>

namespace chimbuko {
  
  /**
     @brief A simple interface for a network in which the sender and receiver are on the same process
  */
  class LocalNet : public NetInterface {
  public:

    //Pointer the the current instance of LocalNet. Only one instance can exist at any given time
    static LocalNet * & globalInstance();

    LocalNet();
    ~LocalNet();    

    /**
     * @brief (virtual) initialize network interface
     * 
     * @param argc command line argc 
     * @param argv command line argv
     * @param nt the number of threads for a thread pool
     */
    void init(int* argc = nullptr, char*** argv = nullptr, int nt=1) override{}

    /**
     * @brief (virtual) finalize network
     * 
     */
    void finalize() override{}

#ifdef _PERF_METRIC
    /**
     * @brief (virtual) Run network server with performance logging to provided directory
     */
    void run(std::string logdir="./") override{}
#else
    /**
     * @brief (virtual) Run network server
     */    
    void run() override{}
#endif
    /**
     * @brief (virtual) stop network server
     * 
     */
    void stop() override{}

    /**
     * @brief (virtual) name of network server
     * 
     * @return std::string name of network server
     */
    std::string name() const override{ return "LocalNet"; }


    /**
     * @brief Send a message and receive the response
     */
    static std::string send_and_receive(const std::string &send_str);
    
  protected:
    void init_thread_pool(int nt){}

  };


}
