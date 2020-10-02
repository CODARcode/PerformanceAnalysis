#pragma once
#include <mutex>
#include <string>
#include <iostream>
#include <list>
#include <nlohmann/json.hpp>
#include "chimbuko/util/RunStats.hpp"

namespace chimbuko {

  /**
   * @brief A class that contains data on the number of anomalies collected during the present timestep. 
   *        It contains the number of anomalies and the timestamp window in which the anomalies occurred
   * 
   */
  class AnomalyData {
  public:
    /**
     * @brief Default constructor
     */
    AnomalyData();

    /**
     * @brief Constructor by value
     * @param app The application index
     * @param rank The MPI rank of the process
     * @param step The io step
     * @param min_ts The first timestamp at which an anomaly was observed in this io step
     * @param max_ts The last timestamp at which an anomaly was observed in this io step
     * @param n_anomalies The number of anomalies observed in this io step
     * @param stat_id An optional string tag attached to this class instance
     */
    AnomalyData(
		unsigned long app, unsigned long rank, unsigned step,
		unsigned long min_ts, unsigned long max_ts, unsigned long n_anomalies,
		std::string stat_id="");

    /**
     * @brief Construct using JSON-serialized data
     */
    AnomalyData(const std::string& s);

    /**
     * @brief Set the parameters
     * @param app The application index
     * @param rank The MPI rank of the process
     * @param step The io step
     * @param min_ts The first timestamp at which an anomaly was observed in this io step
     * @param max_ts The last timestamp at which an anomaly was observed in this io step
     * @param n_anomalies The number of anomalies observed in this io step
     * @param stat_id An optional string tag attached to this class instance
     */
    void set(
	     unsigned long app, unsigned long rank, unsigned step,
	     unsigned long min_ts, unsigned long max_ts, unsigned long n_anomalies,
	     std::string stat_id="");

    /**
     * @brief Get the application index
     */
    unsigned long get_app() const { return m_app; }

    /**
     * @brief Get the MPI rank of the process
     */
    unsigned long get_rank() const { return m_rank; }

    /**
     * @brief Get the io step
     */
    unsigned long get_step() const { return m_step; }

    /**
     * @brief Get the first timestamp at which an anomaly was observed in this io step
     */
    unsigned long get_min_ts() const { return m_min_timestamp; }

    /**
     * @brief Get the last timestamp at which an anomaly was observed in this io step
     */
    unsigned long get_max_ts() const { return m_max_timestamp; }

    /**
     * @brief Get the number of anomalies observed in this io step
     */
    unsigned long get_n_anomalies() const { return m_n_anomalies; }

    /**
     * @brief Get the tag associated with this class instance
     */
    std::string get_stat_id() const { return m_stat_id; }

    /**
     * @brief Comparison operator
     */
    friend bool operator==(const AnomalyData& a, const AnomalyData& b);

    /**
     * @brief Negative comparison operator
     */
    friend bool operator!=(const AnomalyData& a, const AnomalyData& b);

    /**
     * @brief Serialize this instance in JSON format
     */
    nlohmann::json get_json() const;

    /*
     * @brief Serialize this instance in Cereal
     */
    template<class Archive>
    void serialize(Archive & archive){
      archive(m_app, m_rank, m_step, m_min_timestamp, m_max_timestamp, m_n_anomalies, m_stat_id);
    }

  private:
    unsigned long m_app; /**< The application index*/
    unsigned long m_rank; /**< The MPI rank of the process */
    unsigned long m_step; /**< The io step */
    unsigned long m_min_timestamp; /**<  The first timestamp at which an anomaly was observed in this io step*/
    unsigned long m_max_timestamp; /**<  The last timestamp at which an anomaly was observed in this io step*/
    unsigned long m_n_anomalies; /**< The number of anomalies observed in this io step */
    std::string   m_stat_id; /**< An optional string tag attached to this class instance */
  };

  bool operator==(const AnomalyData& a, const AnomalyData& b);
  bool operator!=(const AnomalyData& a, const AnomalyData& b);


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
    RunStats                 m_stats; /** Statistics on the number of anomalies over all ranks*/
    std::list<std::string> * m_data; /**< A list of JSON-formatted strings that represent serializations of the incoming AnomalyData instances since last flush*/
  };

} // end of chimbuko namespace
