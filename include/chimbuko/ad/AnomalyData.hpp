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
     */
    AnomalyData(
		unsigned long app, unsigned long rank, unsigned step,
		unsigned long min_ts, unsigned long max_ts, unsigned long n_anomalies);

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
     */
    void set(unsigned long app, unsigned long rank, unsigned step,
	     unsigned long min_ts, unsigned long max_ts, unsigned long n_anomalies);

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
      archive(m_app, m_rank, m_step, m_min_timestamp, m_max_timestamp, m_n_anomalies);
    }

  private:
    unsigned long m_app; /**< The application index*/
    unsigned long m_rank; /**< The MPI rank of the process */
    unsigned long m_step; /**< The io step */
    unsigned long m_min_timestamp; /**<  The first timestamp at which an anomaly was observed in this io step*/
    unsigned long m_max_timestamp; /**<  The last timestamp at which an anomaly was observed in this io step*/
    unsigned long m_n_anomalies; /**< The number of anomalies observed in this io step */
  };

  bool operator==(const AnomalyData& a, const AnomalyData& b);
  bool operator!=(const AnomalyData& a, const AnomalyData& b);

} // end of chimbuko namespace
