#pragma once

#include <mutex>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <chimbuko/util/RunStats.hpp>
#include <chimbuko/net.hpp>
#include <chimbuko/pserver/PSstatSender.hpp>

namespace chimbuko{

  /**
   * @brief Interface for collection of global counter statistics on parameter server
   */
  class GlobalCounterStats{
  public:
    /**
     * @brief Merge internal statistics with those contained within the JSON-formatted string 'data'
     *
     * For data format see ADLocalCounterStatistics::get_json_state()
     */
    void add_data_json(const std::string& data);

    /**
     * @brief Merge internal statistics with those contained within the Cereal portable binary string 'data'
     *
     * For data format see ADLocalCounterStatistics::State::serialize_cerealpb()
     */
    void add_data_cerealpb(const std::string& data);

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
    MessageKind kind() const{ return MessageKind::COUNTER_STATS; }
    MessageType type() const{ return MessageType::REQ_ADD; }
    void action(Message &response, const Message &message) override{
      check(message);
      if(m_global_counter_stats == nullptr) throw std::runtime_error("Cannot update global counter statistics as stats object has not been linked");
      m_global_counter_stats->add_data_cerealpb(message.buf()); //note, this uses a mutex lock internally
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
      nlohmann::json stats = m_stats->get_json_state();
      into["counter_stats"] = std::move(stats);
    }
  };


};
