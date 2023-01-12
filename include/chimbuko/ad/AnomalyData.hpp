#pragma once
#include <chimbuko_config.h>
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
   * These data are aggregated over rank to form the anomaly_stats.anomaly field of the pserver streaming output
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
    AnomalyData(unsigned long app, unsigned long rank, unsigned step,
		unsigned long min_ts, unsigned long max_ts, unsigned long n_anomalies);

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
     * @brief Get the outlier score statistics
     */
    const RunStats & get_outlier_score_stats() const{ return m_outlier_scores; }


    /**
     * @brief Set the earliest timestamp
     */
    void set_min_ts(unsigned long to){ m_min_timestamp = to; }

    /**
     * @brief Set the last timestamp
     */
    void set_max_ts(unsigned long to){ m_max_timestamp = to; }

    /**
     * @brief Increment the number of anomalies
     */
    void incr_n_anomalies(unsigned long by){ m_n_anomalies += by; }

    /**
     * @brief Add an outlier score to the internal statistics
     */
    void add_outlier_score(double val){ m_outlier_scores.push(val); }

    /**
     * @brief Comparison operator
     */
    friend bool operator==(const AnomalyData& a, const AnomalyData& b);

    /**
     * @brief Negative comparison operator
     */
    friend bool operator!=(const AnomalyData& a, const AnomalyData& b);

   
    /**
     * @brief State struct for serialization
     */
    struct State{      
      unsigned long app;
      unsigned long rank;
      unsigned long step;
      unsigned long min_timestamp;
      unsigned long max_timestamp;
      unsigned long n_anomalies;
      RunStats::State outlier_scores;

      /*
       * @brief Serialize this instance in Cereal
       */
      template<class Archive>
      void serialize(Archive & archive){
	archive(app, rank, step, min_timestamp, max_timestamp, n_anomalies, outlier_scores);
      }

      State(const AnomalyData &p);
      State(){}
      State(const nlohmann::json &j);

      /**
       * Serialize into Cereal portable binary format
       */
      std::string serialize_cerealpb() const;
      
      /**
       * Serialize from Cereal portable binary format
       */     
      void deserialize_cerealpb(const std::string &strstate);

      /**
       * @brief Serialize this instance in JSON format
       */
      nlohmann::json get_json() const;

      /**
       * @brief Set the state from a JSON object
       */
      void set_json(const nlohmann::json &j);
    };
    
    /**
     * @brief Get the state for serialization
     */
    State get_state() const{ return State(*this); }


    /**
     * @brief Serialize the state (internal variables) of this instance in JSON format
     */
    nlohmann::json get_json_state() const{ return get_state().get_json(); }

    /**
     * @brief Get the object in JSON format 
     */
    nlohmann::json get_json() const;

    /**
     * @brief Set the member variables according to the state
     */
    void set_state(const State &state);

    /**
     * @brief Set the member variables according to the state in JSON format
     */
    void set_json_state(const nlohmann::json &j){ set_state(State(j)); }

    /*
     * @brief Serialize this instance in Cereal
     */
    template<class Archive>
    void serialize(Archive & archive){
      archive(m_app, m_rank, m_step, m_min_timestamp, m_max_timestamp, m_n_anomalies, m_outlier_scores);
    }    

    /**
     * Serialize into Cereal portable binary format
     */
    std::string serialize_cerealpb() const;
    
    /**
     * Serialize from Cereal portable binary format
     */     
    void deserialize_cerealpb(const std::string &strstate);

    /**
     * @brief Serialize this class for communication over the network
     */
    std::string net_serialize() const;

    /**
     * @brief Unserialize this class after communication over the network
     */
    void net_deserialize(const std::string &s);
    

  private:
    unsigned long m_app; /**< The application index*/
    unsigned long m_rank; /**< The MPI rank of the process */
    unsigned long m_step; /**< The io step */
    unsigned long m_min_timestamp; /**<  The first timestamp at which an anomaly was observed in this io step*/
    unsigned long m_max_timestamp; /**<  The last timestamp at which an anomaly was observed in this io step*/
    unsigned long m_n_anomalies; /**< The number of anomalies observed in this io step */
    RunStats m_outlier_scores; /**< Statistics on outlier scores in this io step over all functions*/
  };

  bool operator==(const AnomalyData& a, const AnomalyData& b);
  bool operator!=(const AnomalyData& a, const AnomalyData& b);

} // end of chimbuko namespace
