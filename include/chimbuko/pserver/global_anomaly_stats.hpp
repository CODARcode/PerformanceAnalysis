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
    /**< A struct holding statistics information on a function*/
    struct FuncStats{
      std::string func; /**< Func name */
      RunStats func_anomaly; /**< Statistics on number of anomalies*/
      RunStats inclusive; /**< Statistics on number of function timings inclusive of children*/
      RunStats exclusive; /**< Statistics on number of function timings exclusive of children*/
    };

    GlobalAnomalyStats(){}

    /**
     * @brief Merge internal statistics with those contained within the JSON-formatted string 'data'
     */
    void add_anomaly_data_json(const std::string& data);

    /**
     * @brief Merge internal statistics with those contained within the Cereal portable binary  formatted string 'data'
     */
    void add_anomaly_data_cerealpb(const std::string& data);

    /**
     * @brief Get the JSON-formatted string corresponding to the anomaly statistics for a given program/rank
     * @param stat_id A string of the format "<PROGRAM IDX>:<RANK>" (eg "0:1" for program 0, rank 1)
     */
    std::string get_anomaly_stat(const std::string& stat_id) const;

    /**
     * @brief Get the RunStats object corresponding to the anomaly statistics for a given program/rank (throw error if not present)
     * @param stat_id A string of the format "<PROGRAM IDX>:<RANK>" (eg "0:1" for program 0, rank 1)
     */
    RunStats get_anomaly_stat_obj(const std::string& stat_id) const;

    /**
     * @brief Get the number of anomaly data objects collected since the last flush for a given program/rank
     * @param stat_id A string of the format "<PROGRAM IDX>:<RANK>" (eg "0:1" for program 0, rank 1)
     */
    size_t get_n_anomaly_data(const std::string& stat_id) const;

    /**
     * @brief Update internal data to include additional information
     * @parag anom The anomaly data
     */
    void update_anomaly_stat(const AnomalyData &anom);

    /**
     * @brief Update internal data to include additional information
     * @param pid Program index
     * @param id Function index
     * @param name Function name
     * @param n_anomaly The number of anomalies detected
     * @param inclusive Statistics on inclusive timings
     * @param exclusive Statistics on exclusive timings
     */
    void update_func_stat(int pid,
			  unsigned long id, 
			  const std::string& name, 
			  unsigned long n_anomaly,
			  const RunStats& inclusive, 
			  const RunStats& exclusive);

    /**
     * @brief Collect anomaly statistics into JSON object and flush the m_anomaly_stats statistics
     */
    nlohmann::json collect_stat_data();

    /**
     * @brief Collect function statistics into JSON object
     */
    nlohmann::json collect_func_data() const;
    
    /**
     * @brief Collect anomaly statistics and function statistics. Flushes the m_anomaly_stats statistics
     * @return JSON object containing anomaly and function data
     */
    nlohmann::json collect();

  protected:    
    // for global anomaly statistics
    mutable std::mutex m_mutex_anom;
    std::unordered_map<std::string, AnomalyStat> m_anomaly_stats; /**< Global anomaly statistics indexed by a stat_id of form "${app_id}:${rank_id}" */
    // for global function statistics
    mutable std::mutex m_mutex_func;
    std::unordered_map<unsigned long, std::unordered_map<unsigned long, FuncStats> > m_funcstats; /**< Map of program index and function index to statistics on the function*/
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
      m_global_anom_stats->add_anomaly_data_cerealpb(message.buf());
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
