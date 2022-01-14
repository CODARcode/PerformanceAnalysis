#include <chimbuko/pserver/GlobalCounterStats.hpp>
#include <chimbuko/util/map.hpp>

using namespace chimbuko;

GlobalCounterStats::GlobalCounterStats(const GlobalCounterStats &r){
  std::lock_guard<std::mutex> _(r.m_mutex);
  m_counter_stats = r.m_counter_stats;
}

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

void NetPayloadUpdateCounterStats::action(Message &response, const Message &message){
  check(message);
  if(m_global_counter_stats == nullptr) throw std::runtime_error("Cannot update global counter statistics as stats object has not been linked");

  ADLocalCounterStatistics loc(0,0,nullptr);
  loc.net_deserialize(message.buf());
  m_global_counter_stats->add_counter_data(loc); //note, this uses a mutex lock internally
  response.set_msg("", false);
}

void PSstatSenderGlobalCounterStatsPayload::add_json(nlohmann::json &into) const{ 
  nlohmann::json stats = m_stats->get_json_state(); //a JSON array
  if(stats.size())
    into["counter_stats"] = std::move(stats);
}

GlobalCounterStats & GlobalCounterStats::operator+=(const GlobalCounterStats &r){
  std::lock_guard<std::mutex> _(m_mutex);
  std::lock_guard<std::mutex> __(r.m_mutex);
  unordered_map_plus_equals(m_counter_stats, r.m_counter_stats);
}
