#pragma once
#include <chimbuko_config.h>
#include <mutex>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "chimbuko/modules/performance_analysis/pserver/AggregateAnomalyData.hpp"
#include "chimbuko/modules/performance_analysis/pserver/AggregateFuncStats.hpp"
#include <chimbuko/core/net.hpp>
#include <chimbuko/core/pserver/PSstatSender.hpp>
#include <chimbuko/modules/performance_analysis/ad/ADLocalFuncStatistics.hpp>
#include <chimbuko/modules/performance_analysis/pserver/FunctionProfile.hpp>
#include <chimbuko/modules/performance_analysis/pserver/PScommon.hpp>

namespace chimbuko{

  /**
   * @brief Interface for collection of global anomaly statistics on parameter server
   */
  class GlobalAnomalyStats{
  public:
    GlobalAnomalyStats(){}

    /**
     * @brief Copy constructor
     *
     * Thread safe (locks input object)
     */
    GlobalAnomalyStats(const GlobalAnomalyStats & r);

    /**
     * @brief Merge internal statistics with those contained within the input ADLocalFuncStatistics object
     *
     * Thread safe
     */
    void add_anomaly_data(const ADLocalFuncStatistics& data);


    /**
     * @brief Get the JSON-formatted string corresponding to the anomaly statistics (RunStats instance) for a given program/rank
     * @param pid program index
     * @param rid rank
     *
     * Thread safe
     */
    std::string get_anomaly_stat(const int pid, const unsigned long rid) const;

    /**
     * @brief Get the RunStats object corresponding to the anomaly statistics for a given program/rank (throw error if not present)
     * @param pid program index
     * @param rid rank
     * 
     * Thread safe
     */
    RunStats get_anomaly_stat_obj(const int pid, const unsigned long rid) const;


    /**
     * @brief Const accessor to the AggregateAnomalyData instance corresponding to a particular stat_id (throw error if not present)
     * @param pid program index
     * @param rid rank
     *
     * NOT thread safe
     */    
    const AggregateAnomalyData & get_anomaly_stat_container(const int pid, const unsigned long rid) const;

    /**
     * @brief Get the number of anomaly data objects collected since the last flush for a given program/rank
     * @param pid program index
     * @param rid rank
     *
     * Thread safe
     */
    size_t get_n_anomaly_data(const int pid, const unsigned long rid) const;

    /**
     * @brief Update internal data to include additional information
     * @param anom The anomaly data
     *
     * Thread safe
     */
    void update_anomaly_stat(const AnomalyData &anom);

    /**
     * @brief Update internal data to include additional information
     * @param pid Program index
     * @param fid Function index
     * @param name Function name
     * @param n_anomaly The number of anomalies detected
     * @param inclusive Statistics on inclusive timings
     * @param exclusive Statistics on exclusive timings
     *
     * Thread safe
     */
    void update_func_stat(int pid,
			  unsigned long fid, 
			  const std::string& name, 
			  unsigned long n_anomaly,
			  const RunStats& inclusive, 
			  const RunStats& exclusive);

    /**
     * @brief Get the FuncStats object containing the profile information for the specified function
     * @param pid Program index
     * @param fid Function index
     *
     * NOT thread safe
     */
    const AggregateFuncStats & get_func_stats(int pid, unsigned long fid) const;

    /**
     * @brief Collect anomaly statistics into JSON object and flush the m_anomaly_stats statistics
     *
     * Thread safe
     */
    nlohmann::json collect_stat_data();

    /**
     * @brief Collect aggregated function profile statistics into JSON object
     *
     * Thread safe
     */
    nlohmann::json collect_func_data() const;
    
    /**
     * @brief Collect anomaly statistics and function statistics. Flushes the m_anomaly_stats statistics
     * @return JSON object containing anomaly and function data
     *
     * Thread safe
     */
    nlohmann::json collect();
    
    /**
     * @brief Comparison operator
     *
     * NOT thread safe
     */
    bool operator==(const GlobalAnomalyStats &r) const{ return m_anomaly_stats == r.m_anomaly_stats && m_funcstats == r.m_funcstats; }

    /**
     * @brief Inequality operator
     *
     * NOT thread safe
     */
    bool operator!=(const GlobalAnomalyStats &r) const{ return !( *this == r ); }


    /**
     * @brief Combine GlobalAnomalyStats instances
     *
     * Thread safe; locks both this and r when appropriate
     */
    GlobalAnomalyStats & operator+=(const GlobalAnomalyStats & r);

    /**
     * @brief Merge the input GlobalAnomalyStats and then flush it
     *
     * Thread safe
     */   
    void merge_and_flush(GlobalAnomalyStats &r);

    /**
     * @brief Compile profile data into the output
     *
     * Thread safe
     */
    void get_profile_data(FunctionProfile &into) const;


  protected:    
    std::unordered_map<int, std::unordered_map<unsigned long, AggregateAnomalyData> > m_anomaly_stats; /**< Map of program index and rank to the statistics of the number of anomalies per step and the AnomalyData objects that have been added by that AD instance since the last flush */
    std::unordered_map<unsigned long, std::unordered_map<unsigned long, AggregateFuncStats> > m_funcstats; /**< Map of program index and function index to aggregated profile statistics on the function*/
    mutable std::mutex m_mutex_anom; /**< Mutex for global anomaly statistics */
    mutable std::mutex m_mutex_func; /**< Mutex for global function statistics */
  };

  /**
   * @brief Net payload for communicating anomaly stats AD->pserver
   */
  class NetPayloadUpdateAnomalyStats: public NetPayloadBase{
    GlobalAnomalyStats * m_global_anom_stats;
  public:
    NetPayloadUpdateAnomalyStats(GlobalAnomalyStats * global_anom_stats): m_global_anom_stats(global_anom_stats){}
    int kind() const override{ return MessageKind::ANOMALY_STATS; }
    MessageType type() const override{ return MessageType::REQ_ADD; }
    void action(Message &response, const Message &message) override;
  };


  /**
   * @brief Payload object for communicating anomaly data pserver->viz
   */
  class PSstatSenderGlobalAnomalyStatsPayload: public PSstatSenderPayloadBase{
  private:    
    GlobalAnomalyStats *m_stats;
  public:
    PSstatSenderGlobalAnomalyStatsPayload(GlobalAnomalyStats *stats): m_stats(stats){}
    void add_json(nlohmann::json &into) const override;
  };

  /**
   * @brief Payload object for communicating anomaly data pserver->viz that aggregates multiple instances of GlobalAnomalyStats and sends the aggregate data
   */ 
  class PSstatSenderGlobalAnomalyStatsCombinePayload: public PSstatSenderPayloadBase{
  private:    
    std::vector<GlobalAnomalyStats> &m_stats;
  public:
    PSstatSenderGlobalAnomalyStatsCombinePayload(std::vector<GlobalAnomalyStats> &stats): m_stats(stats){}
    void add_json(nlohmann::json &into) const override;
  };



};
