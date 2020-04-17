#pragma once
#include "chimbuko/util/threadPool.hpp"
#include "chimbuko/param.hpp"
#include "chimbuko/global_anomaly_stats.hpp"
#include <string>
#include <thread>

namespace chimbuko {

  /**
   * @brief enum network thread level (for MPI)
   * 
   */
  enum class NetThreadLevel {
    THREAD_MULTIPLE = 3
  };

  /**
   * @brief Network interface class
   * 
   */
  class NetInterface {
  public:
    /**
     * @brief Construct a new Net Interface object
     * 
     */
    NetInterface();

    /**
     * @brief Destroy the Net Interface object
     * 
     */
    virtual ~NetInterface();

    /**
     * @brief (virtual) initialize network interface
     * 
     * @param argc command line argc 
     * @param argv command line argv
     * @param nt the number of threads for a thread pool
     */
    virtual void init(int* argc = nullptr, char*** argv = nullptr, int nt=1) = 0;

    /**
     * @brief (virtual) finalize network
     * 
     */
    virtual void finalize() = 0;

    /**
     * @brief (virtual) run network server
     * 
     */
#ifdef _PERF_METRIC
    virtual void run(std::string logdir="./") = 0;
#else
    virtual void run() = 0;
#endif
    /**
     * @brief (virtual) stop network server
     * 
     */
    virtual void stop() = 0;

    /**
     * @brief (virtual) name of network server
     * 
     * @return std::string name of network server
     */
    virtual std::string name() const = 0;

    /**
     * @brief set parameter (local) storage (i.e. only for this server) 
     * 
     * @param param pointer to parameter storage
     * @param kind parameter kind
     */
    void set_parameter(ParamInterface* param);

    ParamInterface* get_parameter() { return m_param; } 

    /**
     * @brief Link the global anomaly stats object
     */
    void set_global_anomaly_stats(GlobalAnomalyStats * stats){  m_global_anom_stats = stats;  }

    GlobalAnomalyStats * get_global_anomaly_stats(){ return m_global_anom_stats; }

    /**
     * @brief Start sending global anomaly stats to the visualization module (curl)
     */
    void run_stat_sender(std::string url, bool bTest=false);

    /**
     * @brief Stop sending global anomaly stats to the visualization module (curl)
     */
    void stop_stat_sender(int wait_msec=0);

  protected:
    /**
     * @brief initialize thread pool 
     * 
     * @param nt the number threads in the pool
     */
    virtual void init_thread_pool(int nt) = 0;

  protected:
    int              m_nt;    /**< The number of threads in the pool */
    ParamInterface * m_param; /**< Pointer to parameter data used to identify anomalies (storage) */
    GlobalAnomalyStats * m_global_anom_stats; /**< Pointer to global anomaly statistics */

    // thread workder to periodically send anomaly statistics to web server
    // , if it is available.
    std::thread     * m_stat_sender; 
    std::atomic_bool  m_stop_sender;
  };

  namespace DefaultNetInterface
  {
    /**
     * @brief get default network interface for easy usages
     * 
     * @return NetInterface& default network 
     */
    NetInterface& get();
  }

} // end of chimbuko namespace
