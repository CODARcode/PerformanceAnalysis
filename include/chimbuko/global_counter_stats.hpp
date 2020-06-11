#pragma once

#include <mutex>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <chimbuko/util/RunStats.hpp>

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
    void add_data(const std::string& data);

    /**
     * @brief Return a copy of the internal counter statistics
     */
    std::unordered_map<std::string, RunStats> get_stats() const;

  protected:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, RunStats> m_counter_stats; /**< Map of counter name to global statistics*/
  };

};
