#include<chimbuko/modules/performance_analysis/provdb/ProvDBpruneGlobalStats.hpp>
#include<chimbuko/core/util/error.hpp>
#include<limits>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

ProvDBpruneGlobalStats::ProvDBpruneGlobalStats(): first_io_step(INT_MAX), last_io_step(INT_MIN), min_timestamp(ULONG_MAX), max_timestamp(0), score(true), severity(true){}

void ProvDBpruneGlobalStats::push(const nlohmann::json &record){
  first_io_step = std::min(first_io_step, record["io_step"].template get<int>());
  last_io_step = std::max(last_io_step, record["io_step"].template get<int>());
  min_timestamp = std::min(min_timestamp, record["entry"].template get<unsigned long>());
  max_timestamp = std::max(max_timestamp, record["exit"].template get<unsigned long>());
  score.push(record["outlier_score"].template get<double>());
  severity.push(record["outlier_severity"].template get<double>());
  
  std::pair<int,int> key(record["rid"].template get<int>(), record["io_step"].template get<int>());
  auto it = rank_anom_counts.find(key);
  if(it == rank_anom_counts.end()) it = rank_anom_counts.insert({key, 1}).first;
  else ++it->second;
}
nlohmann::json ProvDBpruneGlobalStats::summarize() const{
 // “anomaly_metrics”: statistics on anomalies for this function (object). Note this entry is null if no anomalies were detected
 //      {
 //      “anomaly_count”: statistics on the anomaly count for time steps in which anomalies were detected, as well as the total number of anomalies (RunStats)
 //      “first_io_step”: the first IO step in which an anomaly was detected,
 // 	 “last_io_step”: the last IO step in which an anomaly was detected,
 // 	 “max_timestamp”: the last anomaly’s timestamp,
 // 	 “min_timestamp”: the first anomaly’s timestamp,
 // 	 “score”: statistics on the scores for the anomalies (RunStats),
 // 	 “severity”: statistics on the severity of the anomalies (RunStats),
 // 	},

  nlohmann::json out = nlohmann::json::object();
  out["first_io_step"] = first_io_step;
  out["last_io_step"] = last_io_step;
  out["min_timestamp"] = min_timestamp;
  out["max_timestamp"] = max_timestamp;
  out["score"] = score.get_json();
  out["severity"] = severity.get_json();
  
  RunStats step_counts(true);
  for(auto const &e : rank_anom_counts) step_counts.push(e.second);
  out["anomaly_count"] = step_counts.get_json();
  return out;
}
