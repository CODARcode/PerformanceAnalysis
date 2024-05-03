#pragma once
#include <chimbuko_config.h>
#include <chimbuko/modules/performance_analysis/pserver/AggregateFuncAnomalyMetrics.hpp>

namespace chimbuko {

  /**< An object holding aggregated anomaly metrics on a function for all ranks*/
  struct AggregateFuncAnomalyMetricsAllRanks{
  public:
    AggregateFuncAnomalyMetricsAllRanks(){}

    /**
     * @brief Constructor
     * @param pid Program idx
     * @param fid Function idx
     * @param func Function name
     */
    AggregateFuncAnomalyMetricsAllRanks(int pid, int fid, const std::string &func);

    /**
     * @brief Add the anomaly metrics for a specific rank
     */
    void add(const AggregateFuncAnomalyMetrics &metrics);

    /**
     * @brief Create a JSON object from this instance
     */
    nlohmann::json get_json() const;

    /**
     * @brief Get the program idx
     */
    const int get_pid() const{ return m_pid; }

    /**
     * @brief Get the function idx
     */
    const int get_fid() const{ return m_fid; }
    /**
     * @brief Get the function name
     */
    const std::string &get_func() const{ return m_func; }
    /**
     * @brief Get the statistics on the number of anomalies per io step
     */
    const RunStats & get_count_stats() const{ return m_count; }
    /**
     * @brief Get the statistics on the anomaly scores
     */
    const RunStats & get_score_stats() const{ return m_score; }
    /**
     * @brief Get the statistics on the anomaly scores
     */
    const RunStats & get_severity_stats() const{ return m_severity; }

    /**
     * @brief Get the timestamp of the first event included
     */
    unsigned long get_first_event_ts() const{ return m_first_event_ts; }

    /**
     * @brief Get the timestamp of the last event included
     */
    unsigned long get_last_event_ts() const{ return m_last_event_ts; }

    /**
     * @brief Get the first IO step included
     */
    unsigned long get_first_io_step() const{ return m_first_io_step; }

    /**
     * @brief Get the last IO step included
     */
    unsigned long get_last_io_step() const{ return m_last_io_step; }


    /**
     * @brief Comparison operator
     */
    bool operator==(const AggregateFuncAnomalyMetricsAllRanks &r) const{ return m_pid == r.m_pid && m_fid == r.m_fid && m_func == r.m_func &&
	m_count == r.m_count && m_score == r.m_score && m_severity == r.m_severity && 
	m_first_event_ts == r.m_first_event_ts && m_last_event_ts == r.m_last_event_ts &&
	m_first_io_step == r.m_first_io_step && m_last_io_step == r.m_last_io_step;
    }
    /**
     * @brief Inequality operator
     */
    bool operator!=(const AggregateFuncAnomalyMetricsAllRanks &r) const{ return !( *this == r ); }


  private:
    int m_pid; /**< Program idx */
    int m_fid; /**< Function idx */
    std::string m_func; /**< Func name */
    RunStats m_count; /**< Statistics on the number of anomalies on this function/rank per anomaly step*/
    RunStats m_score; /**< Statistics on anomaly scores accumulated over steps*/
    RunStats m_severity; /**< Statistics on anomaly severity accumulated over steps*/
    unsigned long m_first_event_ts; /**< Timestamp of first event in the aggregated data */
    unsigned long m_last_event_ts; /**< Timestamp of last event in the aggregated data */
    unsigned long m_first_io_step; /**< First IO step in the aggregated data */
    unsigned long m_last_io_step; /**< Last IO step in the aggregated data */		    
  };



}
