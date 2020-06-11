#pragma once

#include<unordered_set>
#include <chimbuko/ad/ADNetClient.hpp>
#include <chimbuko/ad/ADEvent.hpp>
#include <chimbuko/ad/ADCounter.hpp>
#ifdef _PERF_METRIC
#include "chimbuko/util/RunMetric.hpp"
#endif


namespace chimbuko{

  /**
   * @brief A class that gathers local counter statistics and communicates them to the parameter server
   * @param step The current io step
   * @param which_counters The set of counters we are interested in (not all might appear in any given run)
   */
  class ADLocalCounterStatistics{
  public:
    ADLocalCounterStatistics(const int step,
			     const std::unordered_set<std::string> *which_counters			     
#ifdef _PERF_METRIC
			     , RunMetric *perf = nullptr
#endif			 		     
			     ): m_step(step), m_which_counter(which_counters)
#ifdef _PERF_METRIC
			  , m_perf(perf)
#endif
    {}
				


    /**
     * @brief Add counters to internal statistics
     */
    void gatherStatistics(const CountersByIndex_t &cntrs_by_idx);


    /**
     * @brief update (send) counter statistics gathered during this io step to the connected parameter server
     * @param net_client The network client object
     * @return std::pair<size_t, size_t> [sent, recv] message size
     */
    std::pair<size_t, size_t> updateGlobalStatistics(ADNetClient &net_client) const;

#ifdef _PERF_METRIC
    /**
     * @brief Attach a RunMetric object into which performance metrics are accumulated
     */
    void linkPerf(RunMetric* perf){ m_perf = perf; }
#endif

    /**
     * @brief Get the map of counter name to statistics
     */
    const std::unordered_map<std::string , RunStats> &getStats() const{ return m_stats; }
    
    /**
     * @brief Get the JSON object that is sent to the parameter server
     */
    nlohmann::json get_json_state() const;

    /**
     * @brief Set the statistics for a particular counter (must be in the list of counters being collected). Primarily used for testing.
     */
    void setStats(const std::string &counter, const RunStats &to);
    
  protected:
    /**
     * @brief update (send) counter statistics gathered during this io step to the connected parameter server
     * 
     * @param net_client The network client object
     * @param l_stats local statistics
     * @param step step (or frame) number
     * @return std::pair<size_t, size_t> [sent, recv] message size
     */
    static std::pair<size_t, size_t> updateGlobalStatistics(ADNetClient &net_client, const std::string &l_stats, int step);

    int m_step; /**< io step */
    const std::unordered_set<std::string> *m_which_counter; /** The set of counters whose statistics we are accumulating */
    std::unordered_map<std::string , RunStats> m_stats; /**< map of counter to statistics */

#ifdef _PERF_METRIC
    RunMetric *m_perf; /**< Store performance data */
#endif 
  };
  
  




};
