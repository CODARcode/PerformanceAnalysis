#include <chimbuko/pserver/GlobalAnomalyMetrics.hpp>
#include <chimbuko/verbose.hpp>
#include <chimbuko/util/error.hpp>
#include <chimbuko/util/string.hpp>

using namespace chimbuko;


void GlobalAnomalyMetrics::add(GlobalAnomalyMetrics::AnomalyMetricsMapType &into, const ADLocalAnomalyMetrics& data){
  std::lock_guard<std::mutex> _(m_mutex);
  int pid = data.get_pid();
  int rid = data.get_rid();

  //Get/create pid and rank sub-maps
  auto pit = into.find(pid);
  if(pit == into.end()) 
    pit = into.emplace(pid, std::unordered_map<int,  std::unordered_map<int, AggregateFuncAnomalyMetrics> >()).first;
  auto rit = pit->second.find(rid);
  if(rit == pit->second.end())
    rit = pit->second.emplace(rid, std::unordered_map<int, AggregateFuncAnomalyMetrics>()).first;
  
  //Create/append func idx maps to data
  const std::unordered_map<int, FuncAnomalyMetrics> & fmetrics = data.get_metrics();
  for(auto in_fit = fmetrics.begin(); in_fit != fmetrics.end(); in_fit++){
    int fid = in_fit->first;
    auto fit = rit->second.find(fid);
    if(fit == rit->second.end())
      fit = rit->second.emplace(fid, AggregateFuncAnomalyMetrics(pid,rid,fid,in_fit->second.get_func()) ).first;

    //Add the data
    fit->second.add(in_fit->second, data.get_step(), data.get_first_event_ts(), data.get_last_event_ts());
  }
}



void GlobalAnomalyMetrics::add(const ADLocalAnomalyMetrics& data){
  add(m_anomaly_metrics_full, data);
  add(m_anomaly_metrics_recent, data);
}


nlohmann::json GlobalAnomalyMetrics::get_json(){
  std::lock_guard<std::mutex> _(m_mutex);
  
  nlohmann::json out = nlohmann::json::array();

  static size_t _id = 0;

  for(auto const &pelem : m_anomaly_metrics_recent){
    for(auto const &relem : pelem.second){
      for(auto const &felem : relem.second){
	//Get the full statistics that accompany these data
	auto full_pit = m_anomaly_metrics_full.find(pelem.first); 
	if(full_pit == m_anomaly_metrics_full.end()) fatal_error("Unable to find program idx in full statistics!");

	auto full_rit = full_pit->second.find(relem.first); 
	if(full_rit == full_pit->second.end()) fatal_error("Unable to find rank idx in full statistics!");

	auto full_fit = full_rit->second.find(felem.first); 
	if(full_fit == full_rit->second.end()) fatal_error("Unable to find function idx in full statistics!");

	//Generate json output
	nlohmann::json elem;	
	elem["app"] = pelem.first;
	elem["rank"] = relem.first;
	elem["fid"] = felem.first;
	elem["fname"] = felem.second.get_func();
	elem["new_data"] = felem.second.get_json();
	elem["all_data"] = full_fit->second.get_json();
	elem["_id"] = _id++;

	out.push_back(std::move(elem));
      }
    }
  }
  	
  //Flush the recent statistics
  m_anomaly_metrics_recent.clear();

  return out;
}

void NetPayloadUpdateAnomalyMetrics::action(Message &response, const Message &message){
  check(message);
  if(m_global_anom_metrics == nullptr) throw std::runtime_error("Cannot update global anomaly metrics as stats object has not been linked");
  
  ADLocalAnomalyMetrics loc;
  loc.net_deserialize(message.buf());
  
  m_global_anom_metrics->add(loc);
  response.set_msg("", false);
}

void PSstatSenderGlobalAnomalyMetricsPayload::add_json(nlohmann::json &into) const{ 
  nlohmann::json stats = m_metrics->get_json();
  if(stats.size() > 0)
    into["anomaly_metrics"] = std::move(stats);
}