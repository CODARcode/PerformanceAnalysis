#include <chimbuko/ad/ADMonitoring.hpp>
#include <chimbuko/util/error.hpp>
using namespace chimbuko;

void ADMonitoring::extractCounters(const CountersByIndex_t &counters){
  //First step is to check if new counters have appeared from the search list and mark them
  if(m_counterMap == nullptr){
    recoverable_error("Counter map has not been linked");
    return;
  }
  for(auto const &e : *m_counterMap){
    int cid = e.first;
    const std::string &counter_name = e.second;
    if(!m_cid_seen.count(cid)){
      //Is this a counter we are looking for?...
      auto cwit = m_counter_watchlist.find(counter_name);
      if(cwit != m_counter_watchlist.end()){
	//Create a new state entry
	const std::string &field_name = cwit->second;
	auto state_it = m_state.insert(m_state.end(), StateEntry(field_name));
	
	//Add to the lookup tables
	m_cid_state_map[cid] = state_it;
	m_fieldname_state_map[field_name] = state_it;
	
	//Register cid as seen before so we don't check it again
	m_cid_seen.insert(cid);
      }
    }
  }

  //Loop through the cids we are interested in and update the values
  for(auto &se : m_cid_state_map){
    int cid = se.first;
    StateEntry &entry = *se.second;
    
    //Are there any updates for this counter
    auto vit = counters.find(cid);
    if(vit != counters.end()){
      //There are!
      //If there are more than 1 entry we should take the most recent (end of list)
      if(vit->second.size() == 0) fatal_error("Counter is contained in map but has no values??");

      CounterDataListIterator_t cdi = *vit->second.rbegin();
      const CounterData_t &cd = *cdi;
      if(cd.get_tid() != 0) recoverable_error("Expected counter to be on thread 0!");
      entry.value = cd.get_value();
      entry.assigned = true;
      m_timestamp = cd.get_ts(); //overwrite timestamp of update (all monitoring data in a dump should arrive at the same time)
    }
  }

}

bool ADMonitoring::hasState(const std::string &field_name) const{
  auto fit = m_fieldname_state_map.find(field_name);
  if(fit == m_fieldname_state_map.end()) return false;
  
  if(!fit->second->assigned) return false; //state entry exists but has not yet been assigned
  return true;
}

const ADMonitoring::StateEntry & ADMonitoring::getState(const std::string &field_name) const{
  auto fit = m_fieldname_state_map.find(field_name);
  if(fit == m_fieldname_state_map.end()) fatal_error("No entry exists with the field name provided");
  
  if(!fit->second->assigned) fatal_error("State entry exists but has not yet been assigned");
  return *fit->second;
}

void ADMonitoring::addWatchedCounter(const std::string &counter_name, const std::string &field_name){
  m_counter_watchlist[counter_name] = field_name;
}
  
nlohmann::json ADMonitoring::get_json() const{
  nlohmann::json out = nlohmann::json::object(); 
  out["timestamp"] = m_timestamp;
  out["data"] = nlohmann::json::array(); 

  for(auto const &e: m_state)
    out["data"].push_back({ {"field", e.field_name} , {"value", e.value} });
  return out;
}

void ADMonitoring::setDefaultWatchList(){
  addWatchedCounter("Memory Footprint (VmRSS) (KB)", "Memory Footprint (VmRSS) (KB)");
  addWatchedCounter("meminfo:MemAvailable (MB)", "Memory Available (MB)");
  addWatchedCounter("meminfo:MemFree (MB)", "Memory Free (MB)");
  addWatchedCounter("cpu: User %", "CPU Usage User (%)");
  addWatchedCounter("cpu: System %", "CPU Usage System (%)");
  addWatchedCounter("cpu: Idle %", "CPU Usage Idle (%)");
}
