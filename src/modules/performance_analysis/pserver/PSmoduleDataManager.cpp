#include<chimbuko/modules/performance_analysis/pserver/PSmoduleDataManager.hpp>
#include<chimbuko/modules/performance_analysis/pserver/NetPayloadRecvCombinedADdata.hpp>
#include<chimbuko/core/util/error.hpp>
#include<chimbuko/core/verbose.hpp>
#include<fstream>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

void PSmoduleDataManager::appendNetWorkerPayloads(NetInterface &net, int worker_id){
  net.add_payload(new NetPayloadRecvCombinedADdataArray(&m_global_func_stats[worker_id], 
							&m_global_counter_stats[worker_id], 
							&m_global_anom_metrics[worker_id]),worker_id); //each worker thread writes to a separate stats object which are aggregated only at viz send time
  net.add_payload(new NetPayloadGlobalFunctionIndexMapBatched(&m_global_func_index_map),worker_id);
}

void PSmoduleDataManager::appendStatSenderPayloads(PSstatSender &sender){
  sender.add_payload(new PSstatSenderGlobalAnomalyStatsCombinePayload(m_global_func_stats));
  sender.add_payload(new PSstatSenderGlobalCounterStatsCombinePayload(m_global_counter_stats));
  sender.add_payload(new PSstatSenderGlobalAnomalyMetricsCombinePayload(m_global_anom_metrics));
}

void PSmoduleDataManager::sendFinalModuleDataToProvDB(PSglobalProvenanceDBclient &pdb_client, const std::string &prov_outputpath, const PSparamManager &model){
  int nt = this->getNnetWorkers();
  GlobalCounterStats tmp_cstats; for(int i=0;i<nt;i++) tmp_cstats += m_global_counter_stats[i];
  nlohmann::json global_counter_stats_j = tmp_cstats.get_json_state();

  GlobalAnomalyStats tmp_fstats; for(int i=0;i<nt;i++) tmp_fstats += m_global_func_stats[i];
  GlobalAnomalyMetrics tmp_metrics; for(int i=0;i<nt;i++) tmp_metrics += m_global_anom_metrics[i];

  //Generate the function profile
  FunctionProfile profile;
  tmp_fstats.get_profile_data(profile);
  tmp_metrics.get_profile_data(profile);
  nlohmann::json profile_j = profile.get_json();
  
  //Get the AD model in a human-readable format
  nlohmann::json ad_model_j = nlohmann::json::array();
  {
    auto model_map = model.getGlobalParamsCopy()->get_all_algorithm_params();
    auto const &fidx_map = m_global_func_index_map.getFunctionIndexMap();
    for(auto const &r : model_map){
      auto fit = fidx_map.find(r.first);
      if(fit == fidx_map.end()) fatal_error("Could not find function in input map");
      nlohmann::json entry = nlohmann::json::object();
      entry["fid"] = r.first;
      entry["pid"] = fit->second.first;
      entry["func_name"] = fit->second.second;
      entry["model"] = std::move(r.second);
      ad_model_j.push_back(std::move(entry));
    }
  }

  if(pdb_client.isConnected()){
    progressStream << "Pserver: sending final statistics to provDB" << std::endl;
    pdb_client.sendMultipleData(profile_j, "func_stats");
    pdb_client.sendMultipleData(global_counter_stats_j, "counter_stats");
    pdb_client.sendMultipleData(ad_model_j, "ad_model");
  }
  if(prov_outputpath.size() > 0){
    progressStream << "Pserver: writing final statistics to disk at path " << prov_outputpath << std::endl;
    std::ofstream gf(prov_outputpath + "/global_func_stats.json");
    std::ofstream gc(prov_outputpath + "/global_counter_stats.json");
    std::ofstream ad(prov_outputpath + "/ad_model.json");
    gf << profile_j.dump();
    gc << global_counter_stats_j.dump();
    ad << ad_model_j.dump();
  }
}

void PSmoduleDataManager::writeModel(const std::string &filename, const PSparamManager &model){
  std::ofstream out(filename);
  if(!out.good()) fatal_error("Could not write anomaly algorithm parameters to the file provided");
  nlohmann::json out_p;
  out_p["func_index_map"] = m_global_func_index_map.serialize();
  out_p["alg_params"] = model.getGlobalModelJSON();
  out << out_p;
}

void PSmoduleDataManager::restoreModel(PSparamManager &model, const std::string &filename){
  std::ifstream in(filename);
  if(!in.good()) fatal_error("Could not load anomaly algorithm parameters from the file provided");
  nlohmann::json in_p;
  in >> in_p;
  m_global_func_index_map.deserialize(in_p["func_index_map"]);
  model.restoreGlobalModelJSON(in_p["alg_params"]);
}
