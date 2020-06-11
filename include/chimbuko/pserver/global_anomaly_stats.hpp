#pragma once

#include <mutex>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "chimbuko/ad/AnomalyStat.hpp"
#include <chimbuko/net.hpp>
#include <chimbuko/pserver/PSstatSender.hpp>

namespace chimbuko{

  /**
   * @brief Interface for collection of global anomaly statistics on parameter server
   */
  class GlobalAnomalyStats{
  public:
    GlobalAnomalyStats(){}
    ~GlobalAnomalyStats();


    /**
     * @brief Initialize global anomaly stats for a job spanning the given number of MPI ranks
     * @param n_ranks A vector of integers where each entry i gives the number of ranks for program index i
     */
    GlobalAnomalyStats(const std::vector<int>& n_ranks);
    
    /**
     * @brief Clear all collected anomaly statistics and revert to initial stat
     * @param n_ranks A vector of integers where each entry i gives the number of ranks for program index i
     */
    void reset_anomaly_stat(const std::vector<int>& n_ranks);

    /**
     * @brief Merge internal statistics with those contained within the JSON-formatted string 'data'
     */
    void add_anomaly_data(const std::string& data);

    /**
     * @brief Get the JSON-formatted string corresponding to the anomaly statistics for a given program/rank
     * @param stat_id A string of the format "<PROGRAM IDX>:<RANK>" (eg "0:1" for program 0, rank 1)
     */
    std::string get_anomaly_stat(const std::string& stat_id) const;

    /**
     * @brief Get the number of anomalies detected for a given program/rank
     * @param stat_id A string of the format "<PROGRAM IDX>:<RANK>" (eg "0:1" for program 0, rank 1)
     */
    size_t get_n_anomaly_data(const std::string& stat_id) const;

    /**
     * @brief Update internal data to include additional information
     * @param id Function index
     * @param name Function name
     * @param n_anomaly The number of anomalies detected
     * @param inclusive Statistics on inclusive timings
     * @param exclusive Statistics on exclusive timings
     */
    void update_func_stat(unsigned long id, 
			  const std::string& name, 
			  unsigned long n_anomaly,
			  const RunStats& inclusive, 
			  const RunStats& exclusive);

    /**
     * @brief Collect anomaly statistics into JSON object
     */
    nlohmann::json collect_stat_data();

    /**
     * @brief Collect function statistics into JSON object
     */
    nlohmann::json collect_func_data();
    
    /**
     * @brief Collect anomaly statistics and function statistics
     * @return JSON object containing anomaly and function data
     */
    nlohmann::json collect();

  protected:
    // for global anomaly statistics
    std::unordered_map<std::string, AnomalyStat*> m_anomaly_stats; /**< Global anomaly statistics indexed by a stat_id of form "${app_id}:${rank_id}" */
    // for global function statistics
    mutable std::mutex m_mutex_func;
    std::unordered_map<unsigned long, std::string> m_func; /**< Map of index to function name */
    std::unordered_map<unsigned long, RunStats> m_func_anomaly; /**< Map of index to statistics on number of anomalies */
    std::unordered_map<unsigned long, RunStats> m_inclusive; /**< Map of index to statistics on function timings inclusive of children */
    std::unordered_map<unsigned long, RunStats> m_exclusive; /**< Map of index to statistics on function timings exclusive of children */
  };

  /**
   * @brief Net payload for communicating anomaly stats AD->pserver
   */
  class NetPayloadUpdateAnomalyStats: public NetPayloadBase{
    GlobalAnomalyStats * m_global_anom_stats;
  public:
    NetPayloadUpdateAnomalyStats(GlobalAnomalyStats * global_anom_stats): m_global_anom_stats(global_anom_stats){}
    MessageKind kind() const{ return MessageKind::ANOMALY_STATS; }
    MessageType type() const{ return MessageType::REQ_ADD; }
    void action(Message &response, const Message &message) override{
      check(message);
      if(m_global_anom_stats == nullptr) throw std::runtime_error("Cannot update global anomaly statistics as stats object has not been linked");
      m_global_anom_stats->add_anomaly_data(message.buf());
      response.set_msg("", false);
    }
  };


  /**
   * @brief Payload object for communicating anomaly data pserver->viz
   */
  class PSstatSenderGlobalAnomalyStatsPayload: public PSstatSenderPayloadBase{
  private:    
    GlobalAnomalyStats *m_stats;
  public:
    PSstatSenderGlobalAnomalyStatsPayload(GlobalAnomalyStats *stats): m_stats(stats){}
    void add_json(nlohmann::json &into) const override{ 
      nlohmann::json stats = m_stats->collect();
      if(stats.size() > 0)
	into["anomaly_stats"] = std::move(stats);
    }
  };



};
