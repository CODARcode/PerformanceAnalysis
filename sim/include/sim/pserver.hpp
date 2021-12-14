#pragma once
#include <chimbuko_config.h>
#include<chimbuko/param.hpp>
#include<chimbuko/util/curlJsonSender.hpp>
#include<chimbuko/pserver/GlobalAnomalyStats.hpp>
#include<chimbuko/pserver/GlobalCounterStats.hpp>
#include<chimbuko/pserver/GlobalAnomalyMetrics.hpp>

namespace chimbuko_sim{
  using namespace chimbuko;
  
  /**
   * @brief An object that represents the pserver for the purposes of aggregating data over AD instances and writing streaming output to disk
   */
  class pserverSim{
    GlobalAnomalyStats global_func_stats; /**< global anomaly statistics */
    GlobalCounterStats global_counter_stats; /**< global counter statistics */
    GlobalAnomalyMetrics global_anomaly_metrics; /**< global anomaly metrics */
    PSstatSenderGlobalAnomalyStatsPayload anomaly_stats_payload;
    PSstatSenderGlobalCounterStatsPayload counter_stats_payload;
    PSstatSenderGlobalAnomalyMetricsPayload anomaly_metrics_payload;
    LocalNet m_net; /**< The net server */
    ParamInterface* m_ad_params; /**< If using an AD algorithm to analyze the trace, these are the global parameters*/
    curlJsonSender* m_viz; /**< Curl connection to visualization (optional, default disabled)*/
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
     * @brief Stand-in for the anomaly metrics comms from AD -> pserver
     */
    void addAnomalyMetrics(const ADLocalAnomalyMetrics &loc){
      global_anomaly_metrics.add(loc);
    }
    
    /**
     * @brief Enable streaming output to be written to the viz on the given url
     */
    void enableVizOutput(const std::string &url);

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
