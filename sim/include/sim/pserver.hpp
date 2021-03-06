#pragma once

#include<chimbuko/param.hpp>
#include<chimbuko/pserver/global_anomaly_stats.hpp>
#include<chimbuko/pserver/global_counter_stats.hpp>

namespace chimbuko_sim{
  using namespace chimbuko;

  //An object that represents the pserver for the purposes of aggregating data over AD instances and writing streaming output to disk
  class pserverSim{
    GlobalAnomalyStats global_func_stats; //global anomaly statistics
    GlobalCounterStats global_counter_stats; //global counter statistics
    PSstatSenderGlobalAnomalyStatsPayload anomaly_stats_payload;
    PSstatSenderGlobalCounterStatsPayload counter_stats_payload;
    LocalNet m_net; /**< The net server */
    ParamInterface* m_ad_params; /**< If using an AD algorithm to analyze the trace, these are the global parameters*/
  public:
    
    /**
     * @brief Construct the pserver. To use an AD algorithm, the static function ad_algorithm should be changed *before* constructing the pserver
     */
    pserverSim();

    ~pserverSim();
  
    /**
     * @brief Stand-in for the func statistics comms from AD -> pserver
     */
    void addAnomalyData(const ADLocalFuncStatistics &loc){
      global_func_stats.add_anomaly_data(loc);
    }

    /**
     * @brief Stand-in for the counter statistics comms from AD -> pserver
     */
    void addCounterData(const ADLocalCounterStatistics &loc){
      global_counter_stats.add_counter_data(loc);
    }
    
    /**
     * @brief Mirror write functionality of PSstatSender
     */
    void writeStreamingOutput() const;
  };  

  /**
   * @brief Get the shared pserver instance
   */
  inline pserverSim & getPserver(){ static pserverSim ps; return ps; }

};
