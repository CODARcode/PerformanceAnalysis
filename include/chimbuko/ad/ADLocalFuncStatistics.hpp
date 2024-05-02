#pragma once
#include <chimbuko_config.h>
#include <chimbuko/ad/ADNetClient.hpp>
#include <chimbuko/ad/ADEvent.hpp>
#include <chimbuko/ad/ADOutlier.hpp>
#include "chimbuko/core/util/PerfStats.hpp"

#include <chimbuko/ad/AnomalyData.hpp>
#include <chimbuko/ad/FuncStats.hpp>

namespace chimbuko{

  /**
   * @brief A class that gathers local function statistics and communicates them to the parameter server
   */
  class ADLocalFuncStatistics{
  public:
    ADLocalFuncStatistics(): m_perf(nullptr){}

    ADLocalFuncStatistics(const unsigned long program_idx, const unsigned long rank, const int step, PerfStats* perf = nullptr);

    /**
     * @brief Add function executions to internal statistics
     */
    void gatherStatistics(const ExecDataMap_t* exec_data);

    /**
     * @brief Add anomalies to internal statistics
     */
    void gatherAnomalies(const ADExecDataInterface &iface);

    /**
     * @brief update (send) function statistics (#anomalies, incl/excl run times) gathered during this io step to the connected parameter server
     * @param net_client The network client object
     * @return std::pair<size_t, size_t> [sent, recv] message size
     *
     * The message communicated is the string dump of the output of get_json_state()
     */
    std::pair<size_t, size_t> updateGlobalStatistics(ADThreadNetClient &net_client) const;

    
    /**
     * @brief Output the contents of this object in JSON format
     */
    nlohmann::json get_json() const;

    /**
     * @brief Attach a RunMetric object into which performance metrics are accumulated
     */
    void linkPerf(PerfStats* perf){ m_perf = perf; }
    
    /**
     * @brief Access the AnomalyData instance
     */
    const AnomalyData & getAnomalyData() const{ return m_anom_data; }

    /**
     * @brief Set the AnomalyData member
     */
    void setAnomalyData(const AnomalyData &to){ m_anom_data = to; }   

    /**
     * @brief Access the function profile statistics
     */
    const std::unordered_map<unsigned long, FuncStats> & getFuncStats() const{ return m_funcstats; }

    /**
     * @brief Set the function profile statistics
     */   
    void setFuncStats(const std::unordered_map<unsigned long, FuncStats> &to){ m_funcstats = to; }

    /**
     * @brief Serialize using cereal
     */
    template<class Archive>
    void serialize(Archive & archive){
      archive(m_funcstats , m_anom_data);
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
     * @brief Comparison operator
     */
    bool operator==(const ADLocalFuncStatistics &r) const{ return m_anom_data == r.m_anom_data && m_funcstats == r.m_funcstats; }

    /**
     * @brief Inequality operator
     */
    bool operator!=(const ADLocalFuncStatistics &r) const{ return !( *this == r ); }

  protected:

    /**
     * @brief update (send) function statistics (#anomalies, incl/excl run times) gathered during this io step to the connected parameter server
     * 
     * @param net_client The network client object
     * @param l_stats local statistics
     * @param step step (or frame) number
     * @return std::pair<size_t, size_t> [sent, recv] message size
     */
    static std::pair<size_t, size_t> updateGlobalStatistics(ADThreadNetClient &net_client, const std::string &l_stats, int step);

    AnomalyData m_anom_data; /**< AnomalyData instance holding information about the anomalies */
    std::unordered_map<unsigned long, FuncStats> m_funcstats; /**< map of function index to function profile statistics */

    PerfStats *m_perf; /**< Store performance data */
  };
  
  




};
