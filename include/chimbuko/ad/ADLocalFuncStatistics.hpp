#pragma once

#include <chimbuko/ad/ADNetClient.hpp>
#include <chimbuko/ad/ADEvent.hpp>
#include <chimbuko/util/Anomalies.hpp>
#ifdef _PERF_METRIC
#include "chimbuko/util/RunMetric.hpp"
#endif


namespace chimbuko{

  /**
   * @brief A class that gathers local function statistics and communicates them to the parameter server
   */
  class ADLocalFuncStatistics{
  public:
    ADLocalFuncStatistics(const int step
#ifdef _PERF_METRIC
			  , RunMetric *perf = nullptr
#endif			  
			  ): m_step(step), m_min_ts(0), m_max_ts(0)
#ifdef _PERF_METRIC
					 , m_perf(perf)
#endif
    {}

    /**
     * @brief Add function executions to internal statistics
     */
    void gatherStatistics(const ExecDataMap_t* exec_data);

    /**
     * @brief Add anomalies to internal statistics
     */
    void gatherAnomalies(const Anomalies &anom);

    /**
     * @brief update (send) function statistics (#anomalies, incl/excl run times) gathered during this io step to the connected parameter server
     * @param net_client The network client object
     * @return std::pair<size_t, size_t> [sent, recv] message size
     *
     * The message communicated is the string dump of the output of get_json_state()
     */
    std::pair<size_t, size_t> updateGlobalStatistics(ADNetClient &net_client) const;

    
    /**
     * @brief Get the current state as a JSON object
     * @param rank The rank of this AD instance
     *
     * The string dump of this object is the serialized form sent to the parameter server
     */
    nlohmann::json get_json_state(const int rank) const;


#ifdef _PERF_METRIC
    /**
     * @brief Attach a RunMetric object into which performance metrics are accumulated
     */
    void linkPerf(RunMetric* perf){ m_perf = perf; }
#endif

  protected:

    /**
     * @brief update (send) function statistics (#anomalies, incl/excl run times) gathered during this io step to the connected parameter server
     * 
     * @param net_client The network client object
     * @param l_stats local statistics
     * @param step step (or frame) number
     * @return std::pair<size_t, size_t> [sent, recv] message size
     */
    static std::pair<size_t, size_t> updateGlobalStatistics(ADNetClient &net_client, const std::string &l_stats, int step);

    int m_step; /**< io step */
    unsigned long m_min_ts; /**lowest timestamp */
    unsigned long m_max_ts; /**highest timestamp */
    std::unordered_map<unsigned long, std::string> m_func; /**< map of function index to function name */
    std::unordered_map<unsigned long, RunStats> m_inclusive; /**< map of function index to function call time including child calls */
    std::unordered_map<unsigned long, RunStats> m_exclusive; /**< map of function index to function call time excluding child calls */
    std::unordered_map<unsigned long, size_t> m_anomaly_count; /**< map of function index to number of anomalies */
    size_t m_n_anomalies; /**< Number of anomalies in total */

#ifdef _PERF_METRIC
    RunMetric *m_perf; /**< Store performance data */
#endif 
  };
  
  




};
