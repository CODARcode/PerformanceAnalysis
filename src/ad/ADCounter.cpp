#include "chimbuko/ad/ADCounter.hpp"
#include <iostream>

using namespace chimbuko;

void ADCounter::addCounter(const Event_t& event){
  if(!m_counterMap) throw "Counter mapping not linked";

  //If this is the first counter after a flush, we recreate the list
  if(!m_counters) m_counters = new CounterDataList;

  auto it = m_counterMap->find(event.counter_id());
  if(it == m_counterMap->end()) throw "Counter index could not be found in map";

  m_counters->push_back( CounterData_t(event, it->second ) );
}

CounterDataList* ADCounter::flushCounters(){
  CounterDataList* ret = m_counters;
  m_counters = nullptr;
  return ret;
}
