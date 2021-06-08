#include<sim/event_id_map.hpp>

using namespace chimbuko;
using namespace chimbuko_sim;

CallListIterator_t eventIDmap::getCallData(const eventID &event_id) const{
  auto it = m_eventid_map.find(event_id);
  if(it == m_eventid_map.end()) throw std::runtime_error("Call " + event_id.toString() + " not present in map");
  else return it->second;
}

std::pair<CallListIterator_t, CallListIterator_t> eventIDmap::getCallWindowStartEnd(const eventID &event_id, const int win_size) const{
  CallListIterator_t it = getCallData(event_id);
  auto cit = m_thread_exec_map.find(it->get_tid());
  assert(cit != m_thread_exec_map.end());
  CallList_t &cl = const_cast<CallList_t &>(cit->second);

  CallListIterator_t beg = cl.begin();
  CallListIterator_t end = cl.end();

  CallListIterator_t prev_n = it;
  for (unsigned int i = 0; i < win_size && prev_n != beg; i++)
    prev_n = std::prev(prev_n);

  CallListIterator_t next_n = it;
  for (unsigned int i = 0; i < win_size + 1 && next_n != end; i++)
    next_n = std::next(next_n);

  return {prev_n, next_n};
}
