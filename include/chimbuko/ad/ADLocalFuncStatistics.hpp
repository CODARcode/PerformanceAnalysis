#pragma once

#include <chimbuko/ad/ADNetClient.hpp>
#include <chimbuko/ad/ADEvent.hpp>
#include <chimbuko/ad/AnomalyData.hpp>
#include <chimbuko/util/Anomalies.hpp>
#include "chimbuko/util/PerfStats.hpp"
#include "chimbuko/chimbuko.hpp"

namespace chimbuko{

  /**
   * @brief A class that gathers local function statistics and communicates them to the parameter server
   */
  class ADLocalFuncStatistics{
  public:
    /**
     * @brief Data structure containing the data that is sent (in serialized form) to the parameter server
     */
    struct State{
      struct FuncData{
	unsigned long pid; /**< Program index*/
	unsigned long id; /**< Function index*/
	std::string name; /**< Function name*/
	unsigned long n_anomaly; /**< Number of anomalies*/
	RunStats::State inclusive; /**< Inclusive runtime stats*/
	RunStats::State exclusive; /**< Exclusive runtime stats*/
      
	/**
	 * @brief Serialize using cereal
	 */
	template<class Archive>
	void serialize(Archive & archive){
	  archive(pid,id,name,n_anomaly,inclusive,exclusive);
	}
      };
      std::vector<FuncData> func; /** Function stats for each function*/
      AnomalyData anomaly; /** Statistics on overall anomalies */
    
      /**
       * @brief Serialize using cereal
       */
      template<class Archive>
      void serialize(Archive & archive){
	archive(func , anomaly);
      }

      /**
       * Serialize into Cereal portable binary format
       */
      std::string serialize_cerealpb() const;
      
      /**
       * Serialize from Cereal portable binary format
       */     
      void deserialize_cerealpb(const std::string &strstate);
    };    


    ADLocalFuncStatistics(const unsigned long program_idx, const unsigned long rank, const int step, PerfStats* perf = nullptr);

    /**
     * @brief Add function executions to internal statistics
     */
    void gatherStatistics(const ExecDataMap_t* exec_data);

    /**
     * @brief Add anomalies to internal statistics
     */
    void gatherAnomalies(const Anomalies &anom);

    /**
     * @brief update (send) function statistics (#anomalies, incl/excl run times) gathered during this io step to the connected parameter server
     * @param net_client The network client object
     * @return std::pair<size_t, size_t> [sent, recv] message size
     *
     * The message communicated is the string dump of the output of get_json_state()
     */
    std::pair<size_t, size_t> updateGlobalStatistics(ADNetClient &net_client, int rank, std::string pserver_addr) const;

    
    /**
     * @brief Get the current state as a JSON object
     *
     * The string dump of this object is the serialized form sent to the parameter server
     */
    nlohmann::json get_json_state() const;

    /**
     * @brief Get the current state as a state object
     *
     * The string dump of this object is the serialized form sent to the parameter server
     */    
    State get_state() const;

    /**
     * @brief Attach a RunMetric object into which performance metrics are accumulated
     */
    void linkPerf(PerfStats* perf){ m_perf = perf; }

  protected:

    /**
     * @brief update (send) function statistics (#anomalies, incl/excl run times) gathered during this io step to the connected parameter server
     * 
     * @param net_client The network client object
     * @param l_stats local statistics
     * @param step step (or frame) number
     * @return std::pair<size_t, size_t> [sent, recv] message size
     */
    static std::pair<size_t, size_t> updateGlobalStatistics(ADNetClient &net_client, const std::string &l_stats, int step, int rank, std::string pserver_addr);

    int m_step; /**< io step */
    unsigned long m_rank; /**< Rank*/
    unsigned long m_program_idx; /**< Program index*/
    unsigned long m_min_ts; /**< lowest timestamp */
    unsigned long m_max_ts; /**< highest timestamp */
    std::unordered_map<unsigned long, std::string> m_func; /**< map of function index to function name */
    std::unordered_map<unsigned long, RunStats> m_inclusive; /**< map of function index to function call time including child calls */
    std::unordered_map<unsigned long, RunStats> m_exclusive; /**< map of function index to function call time excluding child calls */
    std::unordered_map<unsigned long, size_t> m_anomaly_count; /**< map of function index to number of anomalies */
    size_t m_n_anomalies; /**< Number of anomalies in total */

    PerfStats *m_perf; /**< Store performance data */
  };
  
  




};
