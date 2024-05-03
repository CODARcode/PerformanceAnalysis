#include "chimbuko/modules/performance_analysis/ad/ADCounter.hpp"
#include "chimbuko/core/util/error.hpp"
#include <iostream>

using namespace chimbuko;

void ADCounter::addCounter(const Event_t& event){
  if(!m_counterMap) fatal_error("Counter mapping not linked");

  //If this is the first counter after a flush, we recreate the list
  if(!m_counters) m_counters = new CounterDataListMap_p_t;

  //Get the counter name
  auto it = m_counterMap->find(event.counter_id());
  if(it == m_counterMap->end()) fatal_error("Counter index " + std::to_string(event.counter_id()) + " could not be found in map");

  //Append the counter to the list, and add the iterator to the maps
  auto &count_list_prt = (*m_counters)[event.pid()][event.rid()][event.tid()];
  
  CounterDataListIterator_t cit = count_list_prt.emplace(count_list_prt.end(), event, it->second);
  m_timestampCounterMap[event.pid()][event.rid()][event.tid()][event.ts()].push_back(cit);
  m_countersByIdx[event.counter_id()].push_back(cit);
}

void ADCounter::addCounter(const CounterData_t &cdata){
  //If the counter name map is linked, check the name matches
  if(m_counterMap){
    auto it = m_counterMap->find(cdata.get_counterid());
    if(it == m_counterMap->end()) fatal_error("Counter index " + std::to_string(cdata.get_counterid()) + " could not be found in map");
    if(it->second != cdata.get_countername()) fatal_error("Counter name " + cdata.get_countername() + " does not match the name in the index map, " + it->second);
  }

  //If this is the first counter after a flush, we recreate the list
  if(!m_counters) m_counters = new CounterDataListMap_p_t; 

  auto &count_list_prt = (*m_counters)[cdata.get_pid()][cdata.get_rid()][cdata.get_tid()];
  CounterDataListIterator_t cit = count_list_prt.insert(count_list_prt.end(), cdata);
  m_timestampCounterMap[cdata.get_pid()][cdata.get_rid()][cdata.get_tid()][cdata.get_ts()].push_back(cit);
  m_countersByIdx[cdata.get_counterid()].push_back(cit);
}
  


CounterDataListMap_p_t* ADCounter::flushCounters(){
  CounterDataListMap_p_t* ret = m_counters;
  m_counters = nullptr;
  m_timestampCounterMap.clear();
  m_countersByIdx.clear();
  return ret;
}

std::list<CounterDataListIterator_t> ADCounter::getCountersInWindow(const unsigned long pid, const unsigned long rid, const unsigned long tid,
								    const unsigned long t_start, const unsigned long t_end) const{
  if(t_end < t_start) throw std::runtime_error("ADCounter::getCountersInWindow t_end must be >= t_start");
  std::list<CounterDataListIterator_t> out;

  //Find the counter timestamp map for process/rank/thread. Return empty list if no map entries
  auto pit = m_timestampCounterMap.find(pid);
  if(pit == m_timestampCounterMap.end())
    return out;

  auto rit = pit->second.find(rid);
  if(rit == pit->second.end())
    return out;

  auto tit = rit->second.find(tid);
  if(tit == rit->second.end()) 
    return out;

    
  const auto & the_map = tit->second;

  //Get iterators to start and end of window (this is only log(n) complexity which is why std::map is cool)
  auto start = the_map.lower_bound(t_start);
  auto end = the_map.upper_bound(t_end); //points to first element *greater than value*, so we can use != in iteration over elements

  //Return empty list if nothing in window
  if(start == the_map.end()) return out;
  
  //Populate list
  for(auto it = start; it != end; ++it){ //const_iterator to std::list<CounterDataListIterator_t>, we want all elements
    for(const auto &e : it->second)
      out.push_back(e);
  }
  return out;
}
