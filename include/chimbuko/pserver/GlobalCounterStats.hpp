#pragma once
#include <chimbuko_config.h>
#include <mutex>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <chimbuko/util/RunStats.hpp>
#include <chimbuko/core/net.hpp>
#include <chimbuko/pserver/PSstatSender.hpp>
#include <chimbuko/ad/ADLocalCounterStatistics.hpp>

namespace chimbuko{

  /**
   * @brief Interface for collection of global counter statistics on parameter server
   */
  class GlobalCounterStats{
  public:
    GlobalCounterStats(){}

    /**
     * @brief Copy constructor
     *
     * Thread safe
     */
    GlobalCounterStats(const GlobalCounterStats &r);

    /**
     * @brief Merge internal statistics with those contained within the ADLocalCounterStatistics instance
     *
     * Thread safe
     */
    void add_counter_data(const ADLocalCounterStatistics &loc);

    /**
     * @brief Return a copy of the internal counter statistics
     *
     * Thread safe
     */
    std::unordered_map<unsigned long, std::unordered_map<std::string, RunStats> > get_stats() const;
    
    /**
     * @brief Serialize the state into a JSON object for sending to viz
     *
     * Thread safe
     */
    nlohmann::json get_json_state() const;

    /**
     * @brief Comparison operator
     *
     * NOT thread safe
     */
    bool operator==(const GlobalCounterStats &r) const{ return m_counter_stats == r.m_counter_stats; }

    /**
     * @brief Inequality operator
     *
     * NOT thread safe
     */
    bool operator!=(const GlobalCounterStats &r) const{ return !( *this == r ); }


    /**
     * @brief Merge two instances of GlobalCounterStats
     *
     * Thread safe
     */
    GlobalCounterStats & operator+=(const GlobalCounterStats &r);

  protected:
    mutable std::mutex m_mutex;
    std::unordered_map<unsigned long, std::unordered_map<std::string, RunStats> > m_counter_stats; /**< Map of program index and counter name to global statistics*/
  };

  /**
   * @brief Net payload for communicating counter stats AD->pserver
   */
  class NetPayloadUpdateCounterStats: public NetPayloadBase{
    GlobalCounterStats * m_global_counter_stats;
  public:
    NetPayloadUpdateCounterStats(GlobalCounterStats * global_counter_stats): m_global_counter_stats(global_counter_stats){}
    MessageKind kind() const override{ return MessageKind::COUNTER_STATS; }
    MessageType type() const override{ return MessageType::REQ_ADD; }
    void action(Message &response, const Message &message) override;
  };

  /**
   * @brief Payload object for communicating counter data pserver->viz
   */
  class PSstatSenderGlobalCounterStatsPayload: public PSstatSenderPayloadBase{
  private:    
    GlobalCounterStats *m_stats;
  public:
    PSstatSenderGlobalCounterStatsPayload(GlobalCounterStats *stats): m_stats(stats){}
    void add_json(nlohmann::json &into) const override;
  };

  /**
   * @brief Payload object for communicating counter data pserver->viz that aggregates multiple instances of GlobalAnomalyStats and sends the aggregate data
   */
  class PSstatSenderGlobalCounterStatsCombinePayload: public PSstatSenderPayloadBase{
  private:    
    std::vector<GlobalCounterStats> &m_stats;
  public:
    PSstatSenderGlobalCounterStatsCombinePayload(std::vector<GlobalCounterStats> &stats): m_stats(stats){}
    void add_json(nlohmann::json &into) const override;
  };


};
