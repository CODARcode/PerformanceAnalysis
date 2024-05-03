#pragma once
#include <chimbuko_config.h>
#include "chimbuko/modules/performance_analysis/ad/AnomalyData.hpp"

namespace chimbuko {

  /**
   * @brief A class that contains statistics on the number of anomalies detected
   *
   * It contains the AnomalyData collected over IO steps for a given app/rank 
   */
  class AggregateAnomalyData {
  public:
    AggregateAnomalyData(bool do_accumulate=false);
    ~AggregateAnomalyData();

    AggregateAnomalyData(const AggregateAnomalyData &r);
    AggregateAnomalyData(AggregateAnomalyData &&r);

    AggregateAnomalyData & operator=(const AggregateAnomalyData &r);
    AggregateAnomalyData & operator=(AggregateAnomalyData &&r);

    /**
     * @brief Set the stats object to accumulate the sum total
     */
    void set_do_accumulate(const bool v){ m_stats.set_do_accumulate(v); }

    /**
     * @brief Add the anomaly count from the input AnomalyData instance to the internal statistics
     * @param d The AnomalyData instance
     * @param bStore If true the AnomalyData instance dumped to a JSON-formatted string will be added to the "data list"
     */
    void add(const AnomalyData& d, bool bStore = true);

    /**
     * @brief Get the current statistics
     */
    const RunStats & get_stats() const{ return m_stats; }

    /**
     * @brief Get the pointer to the data list
     */
    std::list<AnomalyData> const* get_data() const{ return m_data; }

    /**
     * @brief Get the current statistics in JSON format
     * @param pid The program index
     * @param rid The rank index
     */
    nlohmann::json get_json(int pid, int rid) const;

    /**
     * @brief Return the number of JSON-formatted strings of serialized incoming AnomalyData since the last flush
     */
    size_t get_n_data() const;

    /**
     * @brief Flush the list of AnomalyData
     */
    void flush();

    /**
     * @brief Comparison operator
     */
    bool operator==(const AggregateAnomalyData &r) const;
    /**
     * @brief Inequality operator
     */
    bool operator!=(const AggregateAnomalyData &r) const{ return !( *this == r ); }

    /**
     * @brief Combine two AggregateAnomalyData instances
     *
     * Note that the m_data lists are simply concatenated and so may be in a different order than if all the data were collected in a single AggregateAnomalyData instance. However we guarantee no ordering in the list anyway; the data are simply appended as an when it comes in from the clients,
     */
    AggregateAnomalyData & operator+=(const AggregateAnomalyData &r);    
    
  private:
    RunStats                 m_stats; /** Statistics on the number of anomalies collected per io step since start of run as well as count of total anomalies*/
    std::list<AnomalyData> * m_data; /**< A list of AnomalyData instances captured since last flush*/
  };

} // end of chimbuko namespace
