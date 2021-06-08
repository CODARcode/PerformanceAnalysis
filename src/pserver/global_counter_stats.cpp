#include <chimbuko/pserver/global_counter_stats.hpp>

using namespace chimbuko;

void GlobalCounterStats::add_counter_data(const ADLocalCounterStatistics &loc){
  std::lock_guard<std::mutex> _(m_mutex);
  auto & pid_stats = m_counter_stats[loc.getProgramIdex()];
  for(auto const &c: loc.getStats())
    pid_stats[c.first] += c.second;
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
