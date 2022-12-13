#include <chimbuko/ad/ADMonitoring.hpp>
#include <chimbuko/util/error.hpp>
#include <fstream>
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
      std::string field_name;
      bool do_add = false;
      size_t pos;

      auto cwit = m_counter_watchlist.find(counter_name);
      if(cwit != m_counter_watchlist.end()){
	//Check the watchlist
	field_name = cwit->second; do_add = true;
      }else if(m_counter_prefix.size() != 0 && (pos=counter_name.find(m_counter_prefix)) == 0){ 
	//Otherwise, if a prefix has been provided, check for counters with this prefix (prefix must be start of counter name)
	field_name = counter_name.substr(m_counter_prefix.size(), std::string::npos); do_add = true;
      }

      //Create new state entry
      if(do_add){
	auto state_it = m_state.insert(m_state.end(), StateEntry(field_name));
	
	//Add to the lookup tables
	m_cid_state_map[cid] = state_it;
	m_fieldname_state_map[field_name] = state_it;	
      }

      //Register cid as seen before so we don't check it again
      m_cid_seen.insert(cid);
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
      //if(cd.get_tid() != 0) recoverable_error("Expected counter to be on thread 0! Info: "+ cd.get_json().dump());    //This doesn't seem to be true any more because Kevin made changes to the monitoring plugin
      entry.value = cd.get_value();
      entry.assigned = true;
      m_timestamp = cd.get_ts(); //overwrite timestamp of update (all monitoring data in a dump should arrive at the same time)
      m_state_set = true;
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
  if(!m_state_set) return nlohmann::json();

  nlohmann::json out = nlohmann::json::object(); 
  out["timestamp"] = m_timestamp;
  out["data"] = nlohmann::json::array(); 

  for(auto const &e: m_state)
    if(e.assigned)
      out["data"].push_back({ {"field", e.field_name} , {"value", e.value} });
  return out;
}

void ADMonitoring::setDefaultWatchList(){
  setCounterPrefix("monitoring: "); //get all counters prefixed by "monitoring: " ; this prefix is set in the default tau_monitoring.json config file
}

void ADMonitoring::parseWatchListFile(const std::string &file){
  clearWatchList();
  std::ifstream f(file.c_str());
  if(f.fail()) fatal_error("Could not open watchlist file");
  nlohmann::json j = nlohmann::json::parse(f);
  if(!j.is_array()) fatal_error("Expect file to contain a JSON array");
  for(int i=0;i<j.size();i++){
    const nlohmann::json &j_i = j[i];
    if(!j_i.is_array() || j_i.size() != 2) fatal_error("Expect array elements to be two-component arrays");
    addWatchedCounter(j_i[0],j_i[1]);
  }
}

std::vector<int> ADMonitoring::getMonitoringCounterIndices() const{
  std::vector<int> out(m_cid_state_map.size());
  size_t i=0;
  for(auto const &e : m_cid_state_map)
    out[i++] = e.first;
  return out; 
}
