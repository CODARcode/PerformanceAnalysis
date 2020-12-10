#pragma once
#include "chimbuko/ad/AnomalyData.hpp"

namespace chimbuko {

  /**
   * @brief A class that contains statistics on the number of anomalies detected
   */
  class AnomalyStat {
  public:
    AnomalyStat(bool do_accumulate=false);
    ~AnomalyStat();

    AnomalyStat(const AnomalyStat &r);
    AnomalyStat(AnomalyStat &&r);

    AnomalyStat & operator=(const AnomalyStat &r);
    AnomalyStat & operator=(AnomalyStat &&r);

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
     * @brief Add the anomaly count from the input string, a JSON-formatted dump of an AnomalyData instance, to the internal statistics
     * @param d The AnomalyData instance
     * @param bStore If true the string will be added to the "data list"
     */
    void add(const std::string& str, bool bStore = true);

    /**
     * @brief Get copy of the current statistics and the pointer 
     *        to data list.
     * 
     * WARN: Once this function is called, the pointer to the 
     * current data list is returned and new (empty) data list is
     * allocated. So, it is callee's responsibility to free the 
     * allocated memory.
     * 
     * @return std::pair<RunStats, std::list<std::string>*> 
     */
    std::pair<RunStats, std::list<std::string>*> get();

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
     * @return std::list<std::string>* 
     */
    std::list<std::string>* get_data();

    /**
     * @brief Return the number of JSON-formatted strings of serialized incoming AnomalyData since the last flush
     */
    size_t get_n_data() const;

  private:
    mutable std::mutex               m_mutex;
    RunStats                 m_stats; /** Statistics on the number of anomalies collected per io step since start of run as well as count of total anomalies*/
    std::list<std::string> * m_data; /**< A list of JSON-formatted strings that represent serializations of the incoming AnomalyData instances since last flush*/
  };

} // end of chimbuko namespace
