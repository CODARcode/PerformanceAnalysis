#pragma once
#include <chimbuko_config.h>
#include <mutex>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "chimbuko/modules/performance_analysis/pserver/AggregateFuncAnomalyMetrics.hpp"
#include <chimbuko/core/pserver/PSstatSender.hpp>
#include <chimbuko/modules/performance_analysis/ad/ADLocalAnomalyMetrics.hpp>
#include <chimbuko/modules/performance_analysis/pserver/FunctionProfile.hpp>
#include <chimbuko/modules/performance_analysis/pserver/PScommon.hpp>

namespace chimbuko{

  /**
   * @brief Interface for collection of global anomaly metrics on parameter server
   */
  class GlobalAnomalyMetrics{
  public:
    typedef std::unordered_map<int, std::unordered_map<int,  std::unordered_map<int, AggregateFuncAnomalyMetrics> > > AnomalyMetricsMapType; /**< Map of program idx, rank and function idx to aggregated metrics */

    GlobalAnomalyMetrics(){}
   

    /**
     * @brief Copy constructor
     *
     * Thread safe
     */
    GlobalAnomalyMetrics(const GlobalAnomalyMetrics &r);


    /**
     * @brief Merge internal statistics with those contained within the input ADLocalFuncStatistics object
     *
     * Thread safe
     */
    void add(const ADLocalAnomalyMetrics& data);

    /**
     * @brief Get the map of program index, rank and function idx to the metrics accumulated since the start of the run
     *
     * NOT thread safe
     */
    const AnomalyMetricsMapType & getToDateMetrics() const{ return m_anomaly_metrics_full; }

    /**
     * @brief Get the map of program index, rank and function idx to the metrics accumulated since the last flush
     *
     * NOT thread safe
     */
    const AnomalyMetricsMapType & getRecentMetrics() const{ return m_anomaly_metrics_recent; }

    /**
     * @brief Create a JSON object from this instance and reset ("flush") the recent metrics
     *
     * Thread safe
     */
    nlohmann::json get_json();

    /**
     * @brief Comparison operator
     *
     * NOT thread safe
     */
    bool operator==(const GlobalAnomalyMetrics &r) const{ return m_anomaly_metrics_recent == r.m_anomaly_metrics_recent && m_anomaly_metrics_full == r.m_anomaly_metrics_full; }

    /**
     * @brief Inequality operator
     *
     * NOT thread safe
     */
    bool operator!=(const GlobalAnomalyMetrics &r) const{ return !(*this == r); }


    /**
     * @brief Combine two instances of GlobalAnomalyMetrics
     *
     * Thread safe
     */
    GlobalAnomalyMetrics & operator+=(const GlobalAnomalyMetrics &r);
    
    /**
     * @brief Merge the input GlobalAnomalyMetrics and then flush it
     *
     * Thread safe
     */   
    void merge_and_flush(GlobalAnomalyMetrics &r);

    /**
     * @brief Compile profile data into the output
     *
     * Thread safe
     */
    void get_profile_data(FunctionProfile &into) const;

  private:

    /**
     * @brief Merge internal statistics contained in 'into' with those contained within the input ADLocalFuncStatistics object
     *
     * Thread safe
     */   
    void add(GlobalAnomalyMetrics::AnomalyMetricsMapType &into, const ADLocalAnomalyMetrics& data);   

    AnomalyMetricsMapType m_anomaly_metrics_full; /**< Map of program index, rank and function idx to the metrics accumulated since the start of the run */
    AnomalyMetricsMapType m_anomaly_metrics_recent; /**< Map of program index, rank and function idx to the metrics accumulated since the last flush */
    mutable std::mutex m_mutex;
  };



  /**
   * @brief Net payload for communicating anomaly metrics AD->pserver
   */
  class NetPayloadUpdateAnomalyMetrics: public NetPayloadBase{
    GlobalAnomalyMetrics * m_global_anom_metrics;
  public:
    NetPayloadUpdateAnomalyMetrics(GlobalAnomalyMetrics * global_anom_metrics): m_global_anom_metrics(global_anom_metrics){}
    int kind() const override{ return MessageKind::ANOMALY_METRICS; }
    MessageType type() const override{ return MessageType::REQ_ADD; }
    void action(Message &response, const Message &message) override;
  };

  /**
   * @brief Payload object for communicating anomaly metrics pserver->viz
   */
  class PSstatSenderGlobalAnomalyMetricsPayload: public PSstatSenderPayloadBase{
  private:    
    GlobalAnomalyMetrics *m_metrics;
  public:
    PSstatSenderGlobalAnomalyMetricsPayload(GlobalAnomalyMetrics *metrics): m_metrics(metrics){}
    void add_json(nlohmann::json &into) const override;
  };

  /**
   * @brief Payload object for communicating anomaly metrics pserver->viz that aggregates multiple instances of GlobalAnomalyStats and sends the aggregate data
   */
  class PSstatSenderGlobalAnomalyMetricsCombinePayload: public PSstatSenderPayloadBase{
  private:    
    std::vector<GlobalAnomalyMetrics> &m_metrics;
  public:
    PSstatSenderGlobalAnomalyMetricsCombinePayload(std::vector<GlobalAnomalyMetrics> &metrics): m_metrics(metrics){}
    void add_json(nlohmann::json &into) const override;
  };


}

