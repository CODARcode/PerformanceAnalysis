#include<chimbuko/pserver/FunctionProfile.hpp>

using namespace chimbuko;

nlohmann::json & FunctionProfile::getFunction(const int pid, const int fid, const std::string &func){
  auto pit = m_profile.find(pid);
  if(pit == m_profile.end())
    pit = m_profile.emplace(pid, std::unordered_map<int, nlohmann::json>()).first;
  auto fit = pit->second.find(fid);
  if(fit == pit->second.end()){
    nlohmann::json entry;
    entry["app"] = pid;
    entry["fid"] = fid;
    entry["fname"] = func;
    entry["runtime_profile"] = nlohmann::json();
    entry["anomaly_metrics"] = nlohmann::json();

    fit = pit->second.emplace(fid, std::move(entry)).first;
  }
  return fit->second;
}


void FunctionProfile::add(const AggregateFuncStats &stats){
  nlohmann::json &entry = getFunction(stats.get_pid(), stats.get_fid(), stats.get_func());
  auto &m = entry["runtime_profile"];
  m["exclusive_runtime"] = stats.get_exclusive().get_json();
  m["inclusive_runtime"] = stats.get_inclusive().get_json();
}



void FunctionProfile::add(const AggregateFuncAnomalyMetricsAllRanks &metrics){
  nlohmann::json &entry = getFunction(metrics.get_pid(), metrics.get_fid(), metrics.get_func());

  auto &m = entry["anomaly_metrics"];
  m = {
    {"first_io_step", metrics.get_first_io_step()},
    {"last_io_step", metrics.get_last_io_step()},
    { "min_timestamp", metrics.get_first_event_ts() },
    { "max_timestamp", metrics.get_last_event_ts() },
    {"anomaly_count", metrics.get_count_stats().get_json()},
    {"score", metrics.get_score_stats().get_json()},
    {"severity", metrics.get_severity_stats().get_json()}
  };
}
    
nlohmann::json FunctionProfile::get_json() const{
  nlohmann::json out = nlohmann::json::array();

  for(auto const &pelem : m_profile)
    for(auto const &felem : pelem.second)
      out.push_back(felem.second);
  return out;
}
