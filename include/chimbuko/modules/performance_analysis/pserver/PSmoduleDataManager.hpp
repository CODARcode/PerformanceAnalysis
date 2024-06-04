#pragma once

#include<chimbuko_config.h>
#include<chimbuko/core/pserver/PSmoduleDataManagerCore.hpp>
#include<chimbuko/modules/performance_analysis/pserver/GlobalAnomalyStats.hpp>
#include<chimbuko/modules/performance_analysis/pserver/GlobalCounterStats.hpp>
#include<chimbuko/modules/performance_analysis/pserver/GlobalAnomalyMetrics.hpp> 
#include<chimbuko/modules/performance_analysis/pserver/PSglobalFunctionIndexMap.hpp>

namespace chimbuko{ 
  class PSmoduleDataManager: public PSmoduleDataManagerCore{
    std::vector<GlobalAnomalyStats> m_global_func_stats; //global anomaly statistics
    std::vector<GlobalCounterStats> m_global_counter_stats; //global counter statistics
    std::vector<GlobalAnomalyMetrics> m_global_anom_metrics; //global anomaly metrics
    PSglobalFunctionIndexMap m_global_func_index_map; //mapping of function name to global index
    
  public:
    PSmoduleDataManager(int net_nworker): m_global_func_stats(net_nworker), m_global_counter_stats(net_nworker), m_global_anom_metrics(net_nworker), PSmoduleDataManagerCore(net_nworker){}

    /**
     * @brief Append net payloads for receipt of module-specific data from OAD instances to the net interface for a particular worker index
     */
    void appendNetWorkerPayloads(NetInterface &net, int worker_id) override;

    /**
     * @brief Append stat-sender payloads for sending collated module-specific data to the viz
     */
    void appendStatSenderPayloads(PSstatSender &sender) override;

    /**
     * @brief Send final data to the provenance database global collections if desired
     * @param pdb_client The provenance database client
     * @param prov_outputpath A string giving a path for writing provenance data directly to disk, used as an alternative if the provDB is not in use. Blank string indicates no output needed
     * @param model The final AD model
     */
    void sendFinalModuleDataToProvDB(PSProvenanceDBclient &pdb_client, const std::string &prov_outputpath, const PSparamManager &model) override;
    
    /**
     * @brief Write and restore the AD model along with the function name map as the model indices may change between runs
     */
    void writeModel(const std::string &filename, const PSparamManager &model) override;
    void restoreModel(PSparamManager &model, const std::string &filename) override;

    /**
     * @brief External access to the index map, for testing
     */
    PSglobalFunctionIndexMap & getIndexMap(){ return m_global_func_index_map; }
  };

};
