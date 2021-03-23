#include <chimbuko/pserver/global_counter_stats.hpp>
#include <chimbuko/ad/ADLocalCounterStatistics.hpp>

using namespace chimbuko;

void GlobalCounterStats::add_data_json(const std::string& data){
  nlohmann::json j = nlohmann::json::parse(data);
  if(!j.count("counters")) throw std::runtime_error("Data string does not contain counters array");

  std::lock_guard<std::mutex> _(m_mutex);
  for(auto c: j["counters"]){
    if(!c.count("name") || !c.count("stats") || !c.count("pid") ) throw std::runtime_error("Data string counters array has unexpected format");    
    RunStats stats = RunStats::from_json_state(c["stats"]);
    m_counter_stats[c["pid"]][c["name"]] += stats;
  }
}

void GlobalCounterStats::add_data_cerealpb(const std::string& data){
  ADLocalCounterStatistics::State j;
  j.deserialize_cerealpb(data);

  std::lock_guard<std::mutex> _(m_mutex);
  for(auto c: j.counters){
    RunStats stats = RunStats::from_state(c.stats);
    m_counter_stats[c.pid][c.name] += stats;
  }
}

std::unordered_map<unsigned long, std::unordered_map<std::string, RunStats> > GlobalCounterStats::get_stats() const{
  std::lock_guard<std::mutex> _(m_mutex);
  std::unordered_map<unsigned long, std::unordered_map<std::string, RunStats> > out = m_counter_stats;
  return out;
}

nlohmann::json GlobalCounterStats::get_json_state() const{
  nlohmann::json jsonObjects = nlohmann::json::array();
  {
    std::lock_guard<std::mutex> _(m_mutex);
    for(auto const &p : m_counter_stats){
      unsigned long pid = p.first;
      for(auto const &c : p.second){
	const std::string &counter = c.first;
	const RunStats &stats = c.second;

	nlohmann::json object;
	object["app"] = pid;
	object["counter"] = counter;
	object["stats"] = stats.get_json();
	jsonObjects.push_back(object);
      }
    }
  }
  return jsonObjects;
}
