#include <chimbuko/pserver/GlobalAnomalyStats.hpp>
#include <chimbuko/verbose.hpp>
#include <chimbuko/ad/ADLocalFuncStatistics.hpp>
#include <chimbuko/util/error.hpp>
#include <chimbuko/util/string.hpp>

using namespace chimbuko;

GlobalAnomalyStats::GlobalAnomalyStats(const GlobalAnomalyStats & r){
  {
    std::lock_guard<std::mutex> _(r.m_mutex_anom);
    m_anomaly_stats = r.m_anomaly_stats;
  }
  {
    std::lock_guard<std::mutex> _(r.m_mutex_func);
    m_funcstats = r.m_funcstats;
  }
}

GlobalAnomalyStats & GlobalAnomalyStats::operator+=(const GlobalAnomalyStats & r){
  {
    std::lock_guard<std::mutex> _(r.m_mutex_anom);
    std::lock_guard<std::mutex> __(m_mutex_anom);
    unordered_map_plus_equals(m_anomaly_stats, r.m_anomaly_stats);
  }
  {
    std::lock_guard<std::mutex> _(r.m_mutex_func);
    std::lock_guard<std::mutex> __(m_mutex_func);
    unordered_map_plus_equals(m_funcstats, r.m_funcstats);
  }
  return *this;
}

void GlobalAnomalyStats::merge_and_flush(GlobalAnomalyStats &r){
  {
    std::lock_guard<std::mutex> _(r.m_mutex_anom);
    std::lock_guard<std::mutex> __(m_mutex_anom);
    unordered_map_plus_equals(m_anomaly_stats, r.m_anomaly_stats);
    
    for(auto &p: r.m_anomaly_stats){
      for(auto &r: p.second){
	r.second.flush();
      }
    }

  }
  {
    std::lock_guard<std::mutex> _(r.m_mutex_func);
    std::lock_guard<std::mutex> __(m_mutex_func);
    unordered_map_plus_equals(m_funcstats, r.m_funcstats);
  }
}

void GlobalAnomalyStats::update_anomaly_stat(const AnomalyData &anom){
  std::lock_guard<std::mutex> _(m_mutex_anom);
  auto pit = m_anomaly_stats.find(anom.get_app());
  if(pit == m_anomaly_stats.end()){
    typename std::unordered_map<int, std::unordered_map<unsigned long, AggregateAnomalyData> >::value_type v(anom.get_app(), std::unordered_map<unsigned long, AggregateAnomalyData>());
    pit = m_anomaly_stats.insert(std::move(v)).first;
  }
  auto rit = pit->second.find(anom.get_rank());
  if(rit == pit->second.end()){
    typename std::unordered_map<unsigned long, AggregateAnomalyData>::value_type v(anom.get_rank(), AggregateAnomalyData(true)); //enable accumulate
    rit = pit->second.insert(std::move(v)).first;
  }
  rit->second.add(anom);
}

void GlobalAnomalyStats::add_anomaly_data(const ADLocalFuncStatistics& data){
  update_anomaly_stat(data.getAnomalyData());

  for (auto const &fp: data.getFuncStats()) {
    const FuncStats &f = fp.second;
    update_func_stat(f.pid, f.id, f.name, f.n_anomaly, f.inclusive, f.exclusive);
  }
}  




const AggregateAnomalyData & GlobalAnomalyStats::get_anomaly_stat_container(const int pid, const unsigned long rid) const{
  auto pit = m_anomaly_stats.find(pid);
  if(pit == m_anomaly_stats.end()) fatal_error("Could not find pid " + std::to_string(pid));
  auto rit = pit->second.find(rid);
  if(rit == pit->second.end()) fatal_error("Could not find rid " + std::to_string(rid));
  return rit->second;
}

RunStats GlobalAnomalyStats::get_anomaly_stat_obj(const int pid, const unsigned long rid) const{
  std::lock_guard<std::mutex> _(m_mutex_anom);
  return get_anomaly_stat_container(pid, rid).get_stats();  
}

std::string GlobalAnomalyStats::get_anomaly_stat(const int pid, const unsigned long rid) const{
  std::lock_guard<std::mutex> _(m_mutex_anom);
  auto pit = m_anomaly_stats.find(pid);
  if(pit == m_anomaly_stats.end()) return "";
  auto rit = pit->second.find(rid);
  if(rit == pit->second.end()) return "";
  return rit->second.get_stats().get_json().dump();
}

size_t GlobalAnomalyStats::get_n_anomaly_data(const int pid, const unsigned long rid) const{
  std::lock_guard<std::mutex> _(m_mutex_anom);
  auto pit = m_anomaly_stats.find(pid);
  if(pit == m_anomaly_stats.end()) return 0;
  auto rit = pit->second.find(rid);
  if(rit == pit->second.end()) return 0;
  return rit->second.get_n_data();
}

nlohmann::json GlobalAnomalyStats::collect_stat_data(){
  nlohmann::json jsonObjects = nlohmann::json::array();
  {
    std::lock_guard<std::mutex> _(m_mutex_anom);

    //m_anomaly_stats is a map of app_idx/rank to AggregateAnomalyData instances
    //AggregateAnomalyData contains statistics on the number of anomalies found per io step and also a set of AnomalyData objects
    //that have been collected from that rank since the last flush
    for(auto & pp : m_anomaly_stats){
      int pid = pp.first; //pid
      for(auto & rp: pp.second){
	unsigned long rid = rp.first; //rank
	
	nlohmann::json object = rp.second.get_json(pid,rid);
	rp.second.flush(); //flush the data
	if(!object.empty())
	  jsonObjects.push_back(std::move(object));
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
	jsonObjects.push_back(f.second.get_json());
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

  //Get stats struct (will create entries if needed)
  auto pit = m_funcstats.find(pid);
  if(pit == m_funcstats.end())
    pit = m_funcstats.emplace(pid, std::unordered_map<unsigned long, AggregateFuncStats>()).first;
  auto fit = pit->second.find(fid);
  if(fit == pit->second.end())
    fit = pit->second.emplace(fid, AggregateFuncStats(pid, fid, name)).first;

  fit->second.add(n_anomaly,inclusive,exclusive);  
}

const AggregateFuncStats & GlobalAnomalyStats::get_func_stats(int pid, unsigned long fid) const{
  auto pit = m_funcstats.find(pid);
  if(pit == m_funcstats.end()) fatal_error("Could not find program index");
  auto fit = pit->second.find(fid);
  if(fit == pit->second.end()) fatal_error("Could not find function index");
  return fit->second;
}


void NetPayloadUpdateAnomalyStats::action(Message &response, const Message &message){
  check(message);
  if(m_global_anom_stats == nullptr) throw std::runtime_error("Cannot update global anomaly statistics as stats object has not been linked");
  
  ADLocalFuncStatistics loc;
  loc.net_deserialize(message.buf());
  
  m_global_anom_stats->add_anomaly_data(loc);
  response.set_msg("", false);
}

void PSstatSenderGlobalAnomalyStatsPayload::add_json(nlohmann::json &into) const{ 
  nlohmann::json stats = m_stats->collect();
  if(stats.size() > 0)
    into["anomaly_stats"] = std::move(stats);
}

void PSstatSenderGlobalAnomalyStatsCombinePayload::add_json(nlohmann::json &into) const{ 
  GlobalAnomalyStats comb;
  for(int i=0;i<m_stats.size();i++)
    comb.merge_and_flush(m_stats[i]);

  nlohmann::json stats = comb.collect();
  if(stats.size() > 0)
    into["anomaly_stats"] = std::move(stats);
}
