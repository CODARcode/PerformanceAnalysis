#include <chimbuko/pserver/global_anomaly_stats.hpp>
#include <chimbuko/verbose.hpp>
#include <chimbuko/ad/ADLocalFuncStatistics.hpp>

using namespace chimbuko;

void GlobalAnomalyStats::update_anomaly_stat(const AnomalyData &anom){
  std::lock_guard<std::mutex> _(m_mutex_anom);
  auto sit = m_anomaly_stats.find(anom.get_stat_id());
  if(sit == m_anomaly_stats.end()){
    m_anomaly_stats[anom.get_stat_id()].set_do_accumulate(true);
    sit = m_anomaly_stats.find(anom.get_stat_id());
  }
  sit->second.add(anom);
}

void GlobalAnomalyStats::add_anomaly_data_json(const std::string& data){
  nlohmann::json j = nlohmann::json::parse(data);
  if (j.count("anomaly")){ 
    AnomalyData d(j["anomaly"].dump());
    update_anomaly_stat(d);
  }
  
  if (j.count("func")){
    for (auto f: j["func"]) {
      update_func_stat(f["pid"], f["id"], f["name"], f["n_anomaly"],
		       RunStats::from_strstate(f["inclusive"].dump()), 
		       RunStats::from_strstate(f["exclusive"].dump())
		       ); //locks
    }
  } 
}



void GlobalAnomalyStats::add_anomaly_data_cerealpb(const std::string& data)
{
  ADLocalFuncStatistics::State j;
  j.deserialize_cerealpb(data);

  const AnomalyData &d = j.anomaly;
  update_anomaly_stat(d);

  for (auto f: j.func) {
    update_func_stat(f.pid, f.id, f.name, f.n_anomaly,
		     RunStats::from_state(f.inclusive), 
		     RunStats::from_state(f.exclusive)
		     ); //locks
  }
}

std::string GlobalAnomalyStats::get_anomaly_stat(const std::string& stat_id) const
{
  if (stat_id.length() == 0 || m_anomaly_stats.count(stat_id) == 0)
    return "";
  
  RunStats stat = m_anomaly_stats.find(stat_id)->second.get_stats();
  return stat.get_json().dump();
}

RunStats GlobalAnomalyStats::get_anomaly_stat_obj(const std::string& stat_id) const
{
  if (stat_id.length() == 0 || m_anomaly_stats.count(stat_id) == 0)
    throw std::runtime_error("GlobalAnomalyStats::get_anomaly_stat_obj stat_id " + stat_id + " does not exist");
  return m_anomaly_stats.find(stat_id)->second.get_stats();
}



size_t GlobalAnomalyStats::get_n_anomaly_data(const std::string& stat_id) const
{
    if (stat_id.length() == 0 || m_anomaly_stats.count(stat_id) == 0)
        return 0;

    return m_anomaly_stats.find(stat_id)->second.get_n_data();
}

nlohmann::json GlobalAnomalyStats::collect_stat_data(){
  nlohmann::json jsonObjects = nlohmann::json::array();
    
  for (auto &pair: m_anomaly_stats){
    std::string stat_id = pair.first;
    auto stats = pair.second.get(); //returns a std::pair<RunStats, std::list<std::string>*>,  and flushes the state of pair.second. 
      //We now own the std::list<std::string>* pointer and have to delete it
      
    if (stats.second && stats.second->size()){
      nlohmann::json object;
      object["key"] = stat_id;
      object["stats"] = stats.first.get_json();

      object["data"] = nlohmann::json::array();
      for (auto strdata: *stats.second)
	{
	  object["data"].push_back(
				   AnomalyData(strdata).get_json()
				   );
	}

      jsonObjects.push_back(object);
      delete stats.second;            
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


void GlobalAnomalyStats::update_func_stat(int pid, unsigned long id, const std::string& name, 
					  unsigned long n_anomaly, const RunStats& inclusive, const RunStats& exclusive){
  std::lock_guard<std::mutex> _(m_mutex_func);
  //Determine if previously seen
  bool first_encounter = false;
  auto pit = m_funcstats.find(pid);
  if(pit == m_funcstats.end() || pit->second.count(id) == 0) 
    first_encounter = true;

  //Get stats struct (will create entries if needed)
  FuncStats &fstats = m_funcstats[pid][id];   

  if(first_encounter){
    fstats.func = name;
    fstats.func_anomaly.set_do_accumulate(true);
  }

  fstats.func_anomaly.push(n_anomaly);
  fstats.inclusive += inclusive;
  fstats.exclusive += exclusive;
}
