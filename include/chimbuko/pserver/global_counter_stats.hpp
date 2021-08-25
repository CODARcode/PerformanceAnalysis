#pragma once
#include <chimbuko_config.h>
#include <mutex>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <chimbuko/util/RunStats.hpp>
#include <chimbuko/net.hpp>
#include <chimbuko/pserver/PSstatSender.hpp>
#include <chimbuko/ad/ADLocalCounterStatistics.hpp>

namespace chimbuko{

  /**
   * @brief Interface for collection of global counter statistics on parameter server
   */
  class GlobalCounterStats{
  public:
    /**
     * @brief Merge internal statistics with those contained within the ADLocalCounterStatistics instance
     */
    void add_counter_data(const ADLocalCounterStatistics &loc);

    /**
     * @brief Return a copy of the internal counter statistics
     */
    std::unordered_map<unsigned long, std::unordered_map<std::string, RunStats> > get_stats() const;
    
    /**
     * @brief Serialize the state into a JSON object for sending to viz
     */
    nlohmann::json get_json_state() const;

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
    void action(Message &response, const Message &message) override{
      check(message);
      if(m_global_counter_stats == nullptr) throw std::runtime_error("Cannot update global counter statistics as stats object has not been linked");

      ADLocalCounterStatistics loc(0,0,nullptr);
      loc.net_deserialize(message.buf());
      m_global_counter_stats->add_counter_data(loc); //note, this uses a mutex lock internally
      response.set_msg("", false);
    }
  };

  /**
   * @brief Payload object for communicating counter data pserver->viz
   */
  class PSstatSenderGlobalCounterStatsPayload: public PSstatSenderPayloadBase{
  private:    
    GlobalCounterStats *m_stats;
  public:
    PSstatSenderGlobalCounterStatsPayload(GlobalCounterStats *stats): m_stats(stats){}
    void add_json(nlohmann::json &into) const override{ 
      nlohmann::json stats = m_stats->get_json_state(); //a JSON array
      if(stats.size())
	into["counter_stats"] = std::move(stats);
    }
  };


};
