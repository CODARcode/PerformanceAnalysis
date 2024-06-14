#pragma once
#include <chimbuko_config.h>
#include <chimbuko/core/ad/ADNetClient.hpp>
#include <chimbuko/modules/performance_analysis/ad/FuncAnomalyMetrics.hpp>
#include <chimbuko/core/ad/ADOutlier.hpp>
#include <chimbuko/modules/performance_analysis/ad/ADExecDataInterface.hpp>

namespace chimbuko{
  namespace modules{
    namespace performance_analysis{
 
      /**
       * @brief A class that collects anomaly metrics broken down by function for sending to the pserver
       */
      class ADLocalAnomalyMetrics{
      public:   

	/**
	 * @brief Constructor
	 * @param app Application index
	 * @param rank AD rank
	 * @param step IO step
	 * @param first_event_ts Timestamp of the first event on this step
	 * @param last_event_ts Timestamp of the last event on this step
	 * @param iface Interface object to labeled data
	 */
	ADLocalAnomalyMetrics(int app, int rank, int step, unsigned long first_event_ts, unsigned long last_event_ts, const ADExecDataInterface &iface);

	ADLocalAnomalyMetrics(){}

	/*
	 * @brief Serialize this instance in Cereal
	 */
	template<class Archive>
	void serialize(Archive & archive){
	  archive(m_app, m_rank, m_step, m_first_event_ts, m_last_event_ts, m_func_anom_metrics);
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


	/**
	 * @brief Send the data to the pserver
	 * @param net_client The network client object
	 * @return std::pair<size_t, size_t> [sent, recv] message size
	 */
	std::pair<size_t, size_t> send(ADNetClient &client) const;
   
	/**
	 * @brief Get the data
	 */
	const std::unordered_map<int, FuncAnomalyMetrics> & get_metrics() const{ return m_func_anom_metrics; }

	/**
	 * @brief Set the data
	 */
	void set_metrics(const std::unordered_map<int, FuncAnomalyMetrics> &to){ m_func_anom_metrics = to; }

	/**
	 * @brief Get the program idx
	 */
	int get_pid() const{ return m_app; }

	/**
	 * @brief Set the program idx
	 */
	void set_pid(int to){ m_app = to; }

	/**
	 * @brief Get the rank
	 */
	int get_rid() const{ return m_rank; }

	/**
	 * @brief Set the rank
	 */
	void set_rid(int to){ m_rank = to; }

	/**
	 * @brief Get the IO step
	 */
	int get_step() const{ return m_step; }

	/**
	 * @brief Get the IO step
	 */
	void set_step(int to){ m_step = to; }

	/**
	 * @brief Get the timestamp of the first event on this IO step
	 */
	unsigned long get_first_event_ts() const{ return m_first_event_ts; }

	/**
	 * @brief Set the timestamp of the first event on this IO step
	 */
	void set_first_event_ts(unsigned long to){ m_first_event_ts = to; }

	/**
	 * @brief Get the timestamp of the last event on this IO step
	 */
	unsigned long get_last_event_ts() const{ return m_last_event_ts; }

	/**
	 * @brief Set the timestamp of the last event on this IO step
	 */
	void set_last_event_ts(unsigned long to){ m_last_event_ts = to; } 
    
	/**
	 * @brief Equivalence operator
	 */
	bool operator==(const ADLocalAnomalyMetrics &r) const{
	  return m_app == r.m_app && m_rank == r.m_rank && m_step == r.m_step && m_func_anom_metrics == r.m_func_anom_metrics && 
	    m_first_event_ts == r.m_first_event_ts && m_last_event_ts == r.m_last_event_ts; 
	}

	/**
	 * @brief Inequality operator
	 */
	inline bool operator!=(const ADLocalAnomalyMetrics &r) const{ return !(*this == r); }

	/**
	 * @brief Attach a RunMetric object into which performance metrics are accumulated
	 */
	void linkPerf(PerfStats* perf){ m_perf = perf; }

      private:
	int m_app; /**< Program idx*/
	int m_rank; /**< rank */
	int m_step; /**< io step */
	unsigned long m_first_event_ts; /**< Timestamp of first event on step */
	unsigned long m_last_event_ts; /**< Timestamp of last event on step */

	std::unordered_map<int, FuncAnomalyMetrics> m_func_anom_metrics; /**< Map of function idx to the metrics for that function*/

	PerfStats *m_perf; /**< Store performance data */
      };
 



    }
  }
}
