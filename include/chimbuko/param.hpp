#pragma once
#include <mutex>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "chimbuko/ad/AnomalyStat.hpp"


namespace chimbuko {

  /** 
   * @brief The general interface for storing function statistics
   */      
  class ParamInterface {
  public:
    ParamInterface();
    ParamInterface(const std::vector<int>& n_ranks);
    virtual ~ParamInterface();

    /**
     * @brief Clear all statistics
     */
    virtual void clear() = 0;

    /**
     * @brief Get the number of functions for which statistics are being collected
    */
    virtual size_t size() const = 0;

    /**
     * @brief Convert internal run statistics to string format for IO
     * @return Run statistics in string format
     */
    virtual std::string serialize() const = 0;

    /**
     * @brief Update the internal run statistics with those included in the serialized input map
     * @param parameters The parameters in serialized format
     * @param flag The meaning of the flag is dependent on the implementation
     * @return Returned contents dependent on implementation
     */
    virtual std::string update(const std::string& parameters, bool flag=false) = 0;

    /**
     * @brief Set the internal run statistics to match those included in the serialized input map. Overwrite performed only for those keys in input.
     * @param runstats The serialized input map
     */
    virtual void assign(const std::string& parameters) = 0;

    virtual void show(std::ostream& os) const = 0;

    // anomaly statistics related ...

    /**
     * @brief Clear all collected anomaly statistics and revert to initial state
     * @param n_ranks A vector of integers where each entry i gives the number of ranks for program index i
     */
    void reset_anomaly_stat(const std::vector<int>& n_ranks);
    void add_anomaly_data(const std::string& data);
    std::string get_anomaly_stat(const std::string& stat_id);
    size_t get_n_anomaly_data(const std::string& stat_id);

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
     * @return JSON-formated string containing anomaly and function data
     */
    std::string collect();

  protected:
    // for parameters of an anomaly detection algorithm
    mutable std::mutex m_mutex; // used to update parameters

    // for global anomaly statistics
    std::unordered_map<std::string, AnomalyStat*> m_anomaly_stats; /**< Global anomaly statistics */
    // for global function statistics
    std::mutex m_mutex_func;
    std::unordered_map<unsigned long, std::string> m_func; /**< Map of index to function name */
    std::unordered_map<unsigned long, RunStats> m_func_anomaly; /**< Map of index to statistics on number of anomalies */
    std::unordered_map<unsigned long, RunStats> m_inclusive; /**< Map of index to statistics on function timings inclusive of children */
    std::unordered_map<unsigned long, RunStats> m_exclusive; /**< Map of index to statistics on function timings exclusive of children */
  };

} // end of chimbuko namespace
