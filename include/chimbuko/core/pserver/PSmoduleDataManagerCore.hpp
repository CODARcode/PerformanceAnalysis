#pragma once

#include<chimbuko_config.h>
#include<chimbuko/core/net.hpp>
#include<chimbuko/core/pserver/PSstatSender.hpp>
#include<chimbuko/core/pserver/PSglobalProvenanceDBclient.hpp>
#include<chimbuko/core/pserver/PSparamManager.hpp>

namespace chimbuko{
  /**
   * @brief A class that maintains module-specific global data (eg statistics) and acts as an intermediary between incoming module-specific data from the OAD and collated data sent to the viz
   */
  class PSmoduleDataManagerCore{
    int m_net_nworker; /**< The number of worker threads used by the net interface*/

  public:    
    PSmoduleDataManagerCore(int net_nworker): m_net_nworker(net_nworker){}

    /**
     * @brief Get the number of worker threads used by the net interface
     */
    int getNnetWorkers() const{ return m_net_nworker; }

    /**
     * @brief Append net payloads for receipt of module-specific data from OAD instances to the net interface for a particular worker index
     *
     * e.g. net.add_payload(new MyNewPayload(&my_module_specific_data),worker_id);
     */
    virtual void appendNetWorkerPayloads(NetInterface &net, int worker_id) = 0;

    /**
     * @brief Append stat-sender payloads for sending collated module-specific data to the viz
     *
     * e.g. sender.add_payload(new MyStatSenderPayload(&my_module_specific_data_for_all_net_workers));     (it is assumed the module-specific data uses appropriate blocking semantics to ensure thread safety)
     */
    virtual void appendStatSenderPayloads(PSstatSender &sender) = 0;

    /**
     * @brief (Optional) Send any final data to the provenance database global collections if desired
     * @param pdb_client The provenance database client
     * @param prov_outputpath A string giving a path for writing provenance data directly to disk, used as an alternative if the provDB is not in use. Blank string indicates no output needed
     * @param model The final AD model
     */
    virtual void sendFinalModuleDataToProvDB(PSglobalProvenanceDBclient &pdb_client, const std::string &prov_outputpath, const PSparamManager &model){}

    /**
     * @brief (Optional) Allow modules to override the formatting of storage of the AD models. This allows storing additional data (e.g. mappings of model indices to internal indices)
     */
    virtual void writeModel(const std::string &filename, const PSparamManager &model){ assert(0 && "Not yet implemented"); }
    virtual void restoreModel(PSparamManager &model, const std::string &filename){ assert(0 && "Not yet implemented"); }

    virtual ~PSmoduleDataManagerCore(){}
  };
};
