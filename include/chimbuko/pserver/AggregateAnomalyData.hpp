#pragma once
#include <chimbuko_config.h>
#include "chimbuko/ad/AnomalyData.hpp"

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
     * @brief Get copy of the current statistics and the pointer 
     *        to data list.
     * 
     * WARN: Once this function is called, the pointer to the 
     * current data list is returned and new (empty) data list is
     * allocated. So, it is callee's responsibility to free the 
     * allocated memory.
     * 
     * @return std::pair<RunStats, std::list<AnomalyData>*> 
     */
    std::pair<RunStats, std::list<AnomalyData>*> get();

    /**
     * @brief Return a copy of current statistics
     *
     * Note: this function does not return a reference because the internal state is constantly changing. 
     *       Here we temporarily lock the state while generating the copy
     */
    RunStats get_stats() const;

    /**
     * @brief Get the pointer to the data list
     * 
     * WARN: As it returns the pointer to the data list,
     * new data can be added to the list in other threads. 
     * Also, it shouldn't be freed by the callee.
     * 
     * @return std::list<AnomalyData>* 
     */
    std::list<AnomalyData>* get_data() const;

    /**
     * @brief Get the current statistics in JSON format and flush the list of AnomalyData collected since previous call to get()
     * @param pid The program index
     * @param rid The rank index
     */
    nlohmann::json get_json_and_flush(int pid, int rid);

    /**
     * @brief Return the number of JSON-formatted strings of serialized incoming AnomalyData since the last flush
     */
    size_t get_n_data() const;

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
    mutable std::mutex               m_mutex;
    RunStats                 m_stats; /** Statistics on the number of anomalies collected per io step since start of run as well as count of total anomalies*/
    std::list<AnomalyData> * m_data; /**< A list of AnomalyData instances captured since last flush*/
  };

} // end of chimbuko namespace
