#pragma once
#include <chimbuko_config.h>
#include<unordered_set>
#include <chimbuko/core/ad/ADNetClient.hpp>
#include <chimbuko/modules/performance_analysis/ad/ADEvent.hpp>
#include <chimbuko/modules/performance_analysis/ad/ADCounter.hpp>
#include "chimbuko/core/util/PerfStats.hpp"

namespace chimbuko{
  namespace modules{
    namespace performance_analysis{


      /**
       * @brief A class that gathers local counter statistics and communicates them to the parameter server
       */
      class ADLocalCounterStatistics{
      public:
	/**
	 * @brief Constructor
	 * @param program_idx The program index
	 * @param step The io step
	 * @param which_counters Pointer to a set of counters that will be collected (not all might appear in any given run). Use nullptr to collect all.
	 * @param perf Attach a PerfStats object into which performance metrics are accumulated
	 */
	ADLocalCounterStatistics(const unsigned long program_idx, const int step,
				 const std::unordered_set<std::string> *which_counters, PerfStats *perf = nullptr):
	  m_program_idx(program_idx), m_step(step), m_which_counter(which_counters), m_perf(perf)
	{}

	ADLocalCounterStatistics(): m_program_idx(0), m_step(-1), m_which_counter(nullptr), m_perf(nullptr){}
	
	/**
	 * @brief Add counters to internal statistics
	 */
	void gatherStatistics(const CountersByIndex_t &cntrs_by_idx);


	/**
	 * @brief update (send) counter statistics gathered during this io step to the connected parameter server
	 * @param net_client The network client object
	 * @return std::pair<size_t, size_t> [sent, recv] message size
	 *
	 * The message string is the output of net_serialize()
	 */
	std::pair<size_t, size_t> updateGlobalStatistics(ADThreadNetClient &net_client) const;

	/**
	 * @brief Attach a PerfStats object into which performance metrics are accumulated
	 */
	void linkPerf(PerfStats* perf){ m_perf = perf; }

	/**
	 * @brief Get the map of counter name to statistics
	 */
	const std::unordered_map<std::string , RunStats> &getStats() const{ return m_stats; }
    
	/**
	 * @brief Get object in the JSON format
	 *
	 */
	nlohmann::json get_json() const;

	/**
	 * @brief Serialize using cereal
	 */
	template<class Archive>
	void serialize(Archive & archive){
	  archive(m_program_idx, m_step, m_stats);
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
	 * @brief Set the statistics for a particular counter (must be in the list of counters being collected). Primarily used for testing.
	 */
	void setStats(const std::string &counter, const RunStats &to);

	/**
	 * @brief Get the program index
	 */
	unsigned long getProgramIndex() const{ return m_program_idx; }

	/**
	 * @brief Get the program index
	 */
	void setProgramIndex(unsigned long to){ m_program_idx = to; }
    
	/**
	 * @brief Get the IO step
	 */
	int getIOstep() const{ return m_step; }

	/**
	 * @brief Set the IO step
	 */   
	void setIOstep(int to){ m_step = to; }

	/**
	 * @brief Comparison operator
	 */
	bool operator==(const ADLocalCounterStatistics &r) const{ return m_program_idx == r.m_program_idx && m_step == r.m_step && m_stats == r.m_stats; }

	/**
	 * @brief Inequality operator
	 */
	bool operator!=(const ADLocalCounterStatistics &r) const{ return !( *this == r ); }

      protected:
	/**
	 * @brief update (send) counter statistics gathered during this io step to the connected parameter server
	 * 
	 * @param net_client The network client object
	 * @param l_stats local statistics
	 * @param step step (or frame) number
	 * @return std::pair<size_t, size_t> [sent, recv] message size
	 */
	static std::pair<size_t, size_t> updateGlobalStatistics(ADThreadNetClient &net_client, const std::string &l_stats, int step);

	unsigned long m_program_idx; /**< Program idx*/
	int m_step; /**< io step */
	const std::unordered_set<std::string> *m_which_counter; /** The set of counters whose statistics we are accumulating */
	std::unordered_map<std::string , RunStats> m_stats; /**< map of counter to statistics */

	PerfStats *m_perf; /**< Store performance data */
      };
  
  




    };
  }
}
