#pragma once

#include "chimbuko/global_anomaly_stats.hpp"
#include <thread>
#include <atomic>

namespace chimbuko{
  
  /**
   * @brief A class that periodically sends aggregate statistics to the visualization module via curl using a background thread
   */
  class PSstatSender{
  public:
    PSstatSender();
    ~PSstatSender();

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

    
  private:
    GlobalAnomalyStats * m_global_anom_stats; /**< Pointer to global anomaly statistics */
   
    // thread workder to periodically send anomaly statistics to web server, if it is available.
    std::thread     * m_stat_sender; 
    std::atomic_bool  m_stop_sender;
  };




};
