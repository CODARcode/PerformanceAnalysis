#pragma once
#include <chimbuko_config.h>
#include <chimbuko/modules/performance_analysis/ad/FuncStats.hpp>

namespace chimbuko {

  /**< An object holding statistics information on a function aggregated over the entire job*/
  struct AggregateFuncStats{
  public:
    /**
     * @brief Default constructor. Note there are no setters for the labels; this function is needed for some STL routines
     */
    AggregateFuncStats(){}
    
    /**
     * @brief Constructor
     * @param pid Program idx
     * @param fid Function idx
     * @param func Function name
     */
    AggregateFuncStats(int pid, int fid, const std::string &func);

    /**
     * @brief Add more data into the accumulated totals
     * @param n_anomaly Number of anomalies
     * @param inclusive Inclusive runtime
     * @param exclusive Exclusive runtime
     */
    void add(unsigned long n_anomaly, const RunStats& inclusive, const RunStats& exclusive);

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
     * @brief Get the statistics on the number of anomalies
     */
    const RunStats & get_func_anomaly() const{ return m_func_anomaly; }
    /**
     * @brief Get the inclusive runtime
     */
    const RunStats & get_inclusive() const{ return m_inclusive; }
    /**
     * @brief Get the exclusive runtime
     */
    const RunStats & get_exclusive() const{ return m_exclusive; }

    /**
     * @brief Comparison operator
     */
    bool operator==(const AggregateFuncStats &r) const{ return m_pid == r.m_pid && m_fid == r.m_fid && m_func == r.m_func &&
	                                                       m_func_anomaly == r.m_func_anomaly && m_inclusive == r.m_inclusive && 
	                                                       m_exclusive == r.m_exclusive; 
    }
    /**
     * @brief Inequality operator
     */
    bool operator!=(const AggregateFuncStats &r) const{ return !( *this == r ); }

    /**
     * @brief Merge two instances of AggregateFuncStats
     */
    AggregateFuncStats & operator+=(const AggregateFuncStats &r);

  private:
    int m_pid; /**< Program idx */
    int m_fid; /**< Function idx */
    std::string m_func; /**< Func name */
    RunStats m_func_anomaly; /**< Statistics on number of anomalies*/
    RunStats m_inclusive; /**< Statistics on number of function timings inclusive of children*/
    RunStats m_exclusive; /**< Statistics on number of function timings exclusive of children*/
  };



}
