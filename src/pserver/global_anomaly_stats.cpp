#include <chimbuko/pserver/global_anomaly_stats.hpp>
#include <chimbuko/verbose.hpp>
#include <chimbuko/ad/ADLocalFuncStatistics.hpp>
#include <chimbuko/util/error.hpp>
#include <chimbuko/util/string.hpp>

using namespace chimbuko;

void GlobalAnomalyStats::update_anomaly_stat(const AnomalyData &anom){
  std::lock_guard<std::mutex> _(m_mutex_anom);
  auto pit = m_anomaly_stats.find(anom.get_app());
  if(pit == m_anomaly_stats.end()){
    typename std::unordered_map<int, std::unordered_map<unsigned long, AnomalyStat> >::value_type v(anom.get_app(), std::unordered_map<unsigned long, AnomalyStat>());
    pit = m_anomaly_stats.insert(std::move(v)).first;
  }
  auto rit = pit->second.find(anom.get_rank());
  if(rit == pit->second.end()){
    typename std::unordered_map<unsigned long, AnomalyStat>::value_type v(anom.get_rank(), AnomalyStat(true)); //enable accumulate
    rit = pit->second.insert(std::move(v)).first;
  }
  rit->second.add(anom);
}

void GlobalAnomalyStats::add_anomaly_data(const ADLocalFuncStatistics& data){
  update_anomaly_stat(data.getAnomalyData());

  for (auto const &fp: data.getFuncStats()) {
    const ADLocalFuncStatistics::FuncStats &f = fp.second;
    update_func_stat(f.pid, f.id, f.name, f.n_anomaly, f.inclusive, f.exclusive);
  }
}  




const AnomalyStat & GlobalAnomalyStats::get_anomaly_stat_container(const int pid, const unsigned long rid) const{
  auto pit = m_anomaly_stats.find(pid);
  if(pit == m_anomaly_stats.end()) fatal_error("Could not find pid " + std::to_string(pid));
  auto rit = pit->second.find(rid);
  if(rit == pit->second.end()) fatal_error("Could not find rid " + std::to_string(rid));
  return rit->second;
}

RunStats GlobalAnomalyStats::get_anomaly_stat_obj(const int pid, const unsigned long rid) const{
  return get_anomaly_stat_container(pid, rid).get_stats();  
}

std::string GlobalAnomalyStats::get_anomaly_stat(const int pid, const unsigned long rid) const{
  RunStats stat;
  try{
    stat = get_anomaly_stat_obj(pid,rid);
  }catch(const std::exception &e){
    return "";
  }
  return stat.get_json().dump();
}

size_t GlobalAnomalyStats::get_n_anomaly_data(const int pid, const unsigned long rid) const{
  AnomalyStat const* s;
  try{
    s = &get_anomaly_stat_container(pid,rid);
  }catch(const std::exception &e){
    return 0;
  }  
  return s->get_n_data();
}

nlohmann::json GlobalAnomalyStats::collect_stat_data(){
  nlohmann::json jsonObjects = nlohmann::json::array();

  //m_anomaly_stats is a map of app_idx/rank to AnomalyStat instances
  //AnomalyStat contains statistics on the number of anomalies found per io step and also a set of AnomalyData objects
  //that have been collected from that rank since the last flush
  for(auto & pp : m_anomaly_stats){
    int pid = pp.first;
    for(auto & rp: pp.second){
      unsigned long rid = rp.first;
      
      auto stats = rp.second.get(); //returns a std::pair<RunStats, std::list<AnomalyData>*>,  and flushes the state of pair.second. 
      //We now own the std::list<AnomalyData>* pointer and have to delete it
      
      if(stats.second){
	if(stats.second->size()){
	  nlohmann::json object;
	  object["key"] = stringize("%d:%d", pid,rid);
	  object["stats"] = stats.first.get_json();
	  
	  object["data"] = nlohmann::json::array();
	  for (auto const &adata: *stats.second){
	    object["data"].push_back(adata.get_json());
	  }
	  jsonObjects.push_back(object);
	}
	delete stats.second;
      }
    }
  }

  return jsonObjects;
}

nlohmann::json GlobalAnomalyStats::collect_func_data() const{
  nlohmann::json jsonObjects = nlohmann::json::array();
  {
    std::lock_guard<std::mutex> _(m_mutex_func);
    for(const auto &p : m_funcstats){
      unsigned long pid = p.first;
      for(const auto &f : p.second){
	unsigned long fid = f.first;
	const FuncStats &fstat = f.second;

	nlohmann::json object;
	object["app"] = pid;
	object["fid"] = fid;
	object["name"] = fstat.func;
	object["stats"] = fstat.func_anomaly.get_json();
	object["inclusive"] = fstat.inclusive.get_json();
	object["exclusive"] = fstat.exclusive.get_json();
	jsonObjects.push_back(object);
      }
    }
  }
  return jsonObjects;
}

nlohmann::json GlobalAnomalyStats::collect(){
    nlohmann::json object;
    object["anomaly"] = collect_stat_data();
    if (object["anomaly"].size() == 0)
      return nlohmann::json(); //return empty object
    object["func"] = collect_func_data();
    object["created_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch() ).count();
    return object;
}


void GlobalAnomalyStats::update_func_stat(int pid, unsigned long fid, const std::string& name, 
					  unsigned long n_anomaly, const RunStats& inclusive, const RunStats& exclusive){
  std::lock_guard<std::mutex> _(m_mutex_func);
  //Determine if previously seen
  bool first_encounter = false;
  auto pit = m_funcstats.find(pid);
  if(pit == m_funcstats.end() || pit->second.count(fid) == 0) 
    first_encounter = true;

  //Get stats struct (will create entries if needed)
  FuncStats &fstats = m_funcstats[pid][fid];   

  if(first_encounter){
    fstats.func = name;
    fstats.func_anomaly.set_do_accumulate(true);
  }

  fstats.func_anomaly.push(n_anomaly);
  fstats.inclusive += inclusive;
  fstats.exclusive += exclusive;
}

const GlobalAnomalyStats::FuncStats & GlobalAnomalyStats::get_func_stats(int pid, unsigned long fid) const{
  std::lock_guard<std::mutex> _(m_mutex_func);
  auto pit = m_funcstats.find(pid);
  if(pit == m_funcstats.end()) fatal_error("Could not find program index");
  auto fit = pit->second.find(fid);
  if(fit == pit->second.end()) fatal_error("Could not find function index");
  return fit->second;
}
