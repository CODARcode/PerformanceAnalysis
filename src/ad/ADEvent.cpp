#include "chimbuko/verbose.hpp"
#include "chimbuko/ad/ADEvent.hpp"
#include "chimbuko/util/map.hpp"
#include "chimbuko/util/error.hpp"
#include <iostream>
#include <sstream>

using namespace chimbuko;

ADEvent::ADEvent(bool verbose)
  : m_funcMap(nullptr), m_eventType(nullptr), m_counterMap(nullptr), m_gpu_thread_Map(nullptr), m_verbose(verbose),
    m_eidx_func_entry(-1), m_eidx_func_exit(-1), m_eidx_comm_send(-1), m_eidx_comm_recv(-1)
{

}

ADEvent::~ADEvent() {
  clear();
}

void ADEvent::clear() {
}

EventError ADEvent::addEvent(const Event_t& event) {
  switch (event.type())
    {
    case EventDataType::FUNC: return addFunc(event);
    case EventDataType::COMM: return addComm(event);
    case EventDataType::COUNT: return addCounter(event);
    default: return EventError::UnknownEvent;
    }
}

void ADEvent::stackProtectGC(CallListIterator_t it){
  it->can_delete(false);

  eventID parent = it->get_parent();
  while(parent != eventID::root()){
    CallListIterator_t pit;
    try{
      pit = getCallData(parent);
    }catch(const std::exception &e){
      recoverable_error("Could not find parent " + parent.toString() + " in call list due to : " + e.what());
      break;
    }

    pit->can_delete(false);
    parent = pit->get_parent();
  }
}

void ADEvent::stackUnProtectGC(CallListIterator_t it){
  //Check if has unmatched correlation iD
  auto umit = m_unmatchedCorrelationID_count.find(it->get_id());
  if(umit == m_unmatchedCorrelationID_count.end() || umit->second == 0)
    it->can_delete(true);
  else return; //stop here, the stack will be needed

  eventID parent = it->get_parent();
  while(parent != eventID::root()){
    CallListIterator_t pit;
    try{
      pit = getCallData(parent);
    }catch(const std::exception &e){
      recoverable_error("Could not find parent " + parent.toString() + " in call list due to : " + e.what());
      break;
    }

    umit = m_unmatchedCorrelationID_count.find(pit->get_id());
    if(umit == m_unmatchedCorrelationID_count.end() || umit->second == 0)
      pit->can_delete(true);
    else break; //stop here, the stack will be needed

    parent = pit->get_parent();
  }
}




void ADEvent::checkAndMatchCorrelationID(CallListIterator_t it){
  //Check if the event has a correlation ID counter, if so try to match it to an outstanding unmatched
  //event with a correlation ID
  int n_cid = 0;
  for(auto const &c : it->get_counters()){
    if(c.get_countername() == "Correlation ID"){
      unsigned long cid = c.get_value();

      //Does a partner already exist?
      auto m = m_unmatchedCorrelationID.find(cid);
      if(m != m_unmatchedCorrelationID.end()){
	eventID current_event_id = it->get_id();
	eventID partner_event_id = m->second->get_id();
	it->set_GPU_correlationID_partner(partner_event_id);
	m->second->set_GPU_correlationID_partner(current_event_id);

	size_t rem = --m_unmatchedCorrelationID_count[partner_event_id];
	if(rem == 0){
	  m_unmatchedCorrelationID_count.erase(partner_event_id); //no need to keep this around now
	  stackUnProtectGC(m->second);
	}
	m_unmatchedCorrelationID.erase(cid); //remove now-matched correlation ID

	verboseStream << "Found partner event " << current_event_id.toString() << " to previous unmatched event " << partner_event_id.toString() << " with correlation ID " << cid << std::endl;
      }else{
	//Ensure the event and it's parental line can't be deleted and put it in the map of unmatched events
	stackProtectGC(it);
	m_unmatchedCorrelationID[cid] = it;
	++m_unmatchedCorrelationID_count[it->get_id()];
	verboseStream << "Found as-yet unpartnered event with correlation ID " << cid << std::endl;
      }
      n_cid++;
    }
  }

  if(n_cid > 1 && m_gpu_thread_Map && m_gpu_thread_Map->count(it->get_tid()) ){
    std::stringstream ss;
    ss << "Encountered a GPU kernel execution with multiple correlation IDs! Execution details:" << std::endl
       << it->get_json(false, true).dump() << std::endl
       << "GPU details: " << std::endl
       << m_gpu_thread_Map->find(it->get_tid())->second.get_json().dump() << std::endl;
    recoverable_error(ss.str());
  }
}


CallListIterator_t ADEvent::addCall(const ExecData_t &exec){
  CallList_t& cl = m_callList[exec.get_pid()][exec.get_rid()][exec.get_tid()];
  CallListIterator_t it = cl.emplace(cl.end(),exec);
  m_execDataMap[it->get_fid()].push_back(it);
  m_callIDMap[it->get_id()] = it;
  checkAndMatchCorrelationID(it);
  return it;
}


EventError ADEvent::addFunc(const Event_t& event) {
  //Determine the event type. Use the known event indices if previously determined, otherwise find them
  if (m_eventType == nullptr) {
    std::cerr << "Uninitialized eventType\n";
    return EventError::UnknownEvent;
  }
  int eid = static_cast<int>(event.eid());

  bool is_entry(false), is_exit(false);

  if(eid == m_eidx_func_entry){
    is_entry = true;
  }else if(eid == m_eidx_func_exit){
    is_exit = true;
  }else{
    //Event might be an unknown type *or* the m_eidx* members have not yet been set
    auto it = m_eventType->find(eid);
    if(it == m_eventType->end()){ //event index is not in the map??
      std::cerr << "Unknown event in eventType: " << eid << std::endl;
      return EventError::UnknownEvent;
    }
    if(m_eidx_func_entry == -1 && it->second == "ENTRY"){
      m_eidx_func_entry = eid;
      is_entry = true;
    }else if(m_eidx_func_exit == -1 && it->second == "EXIT"){
      m_eidx_func_exit = eid;
      is_exit = true;
    }
  }

  //Get the iterator to the function name
  if (m_funcMap == nullptr) {
    std::cerr << "Uninitialized function map\n";
    return EventError::UnknownEvent;
  }
  auto func_name_it = m_funcMap->find(event.fid());

  if(func_name_it == m_funcMap->end()){
    std::cerr << "Unknown function event\n";
    return EventError::UnknownFunc;
  }
  const std::string &func_name = func_name_it->second;

  if(is_entry){
    //Create a new ExecData_t object with function entry information and push onto the call list
    CallList_t& cl = m_callList[event.pid()][event.rid()][event.tid()];

    CallListIterator_t it = cl.emplace(cl.end(),event);

    //Push the function call onto the stack, let the previous top function know it has a child and tell the new function call who its parent is
    CallStack_t& cs = m_callStack[event.pid()][event.rid()][event.tid()];
    if (cs.size()) {
      it->set_parent(cs.top()->get_id());
      cs.top()->inc_n_children();
    }
    it->set_funcname(func_name);
    cs.push(it);

    //Add the new call to the map of call index string
    m_callIDMap[it->get_id()] = it;

    return EventError::OK;
  }else if(is_exit){
    CallStack_t& cs = m_callStack[event.pid()][event.rid()][event.tid()];
    if (cs.size() == 0) { //Expect to have at least one entry; that created when the ENTRY was encountered
      std::stringstream ss;
      ss << "\n***** Empty call stack! *****" << std::endl
	 << "Event information: " << event.get_json().dump() << std::endl
	 << "Event type: EXIT  Function name: " << func_name << std::endl;
      recoverable_error(ss.str());
      return EventError::EmptyCallStack;
    }

    //Get the function call object at the top of the stack and add the exit event
    CallListIterator_t& it = cs.top();
    if (!it->update_exit(event)) {
      std::stringstream ss;
      ss << "\n***** Invalid EXIT event! *****" << std::endl
	 << "Event information: " << event.get_json().dump() << std::endl
	 << "Event type: EXIT  Function name: " << func_name << std::endl
	 << "This can happen if the exit timestamp comes before the entry timestamp or if the function ids don't match" << std::endl
	 << "Exit timestamp: " << event.ts() << " function id of exit event: " << event.fid() << std::endl
	 << "Entry timestamp: " << it->get_entry() << " function id of entry event: " << it->get_fid() << std::endl;
      recoverable_error(ss.str());
      return EventError::CallStackViolation;
    }
    //Remove the object from the stack (it still lives in the CallList)
    cs.pop(); //I'm surprised this doesn't invalidate the iterator
    //Tell the former parent function to subtract the child runtime from its exclusive runtime
    if (!cs.empty()) {
      cs.top()->update_exclusive(it->get_runtime());
    }

    {
      //Associate all comms events on the stack with the call object
      //Sometimes Tau sends comm events out of order with the func events on io step boundaries
      //Prevent this by reinserting onto the stack comm events with a timestamp larger than the exit event
      std::vector<CommData_t> reinsert;

      //Flush the entire comm stack including events that occurred before the func entry (these can no longer be used)
      CommStack_t& comm = m_commStack[event.pid()][event.rid()][event.tid()];
      while (!(comm.empty() || comm.top().ts() < it->get_entry()) ) { //stop if we encounter a comms with a ts before this function's entry as these comms will belong to the parent
	if(comm.top().ts() > event.ts()) reinsert.push_back(comm.top());
	else it->add_message(comm.top(), ListEnd::Front); //stack access is reverse time order! Only does anything if event inside time window
	comm.pop();
      }
      for(auto rit = reinsert.rbegin(); rit != reinsert.rend(); rit++){
	verboseStream << "Warning: Reinserting " << rit->get_json().dump() << " onto comm stack (Tau likely provided this event out of order)" << std::endl;
	comm.push(*rit); //reinsert in reverse order (oldest first)
      }
    }

    //Associate all counter events on the stack with the call object
    {
      //Treat the same as the comm events above
      std::vector<CounterData_t> reinsert;

      CounterStack_t& count = m_counterStack[event.pid()][event.rid()][event.tid()];
      while (!(count.empty() || count.top().get_ts() < it->get_entry()) ) { //stop if we encounter a counter with a ts before this function's entry as these counters will belong to the parent
	if(count.top().get_ts() > event.ts()) reinsert.push_back(count.top());
	else{
	  bool accept = it->add_counter(count.top(), ListEnd::Front); //stack access is reverse time order! Only does anything if event inside time window
	  if(!accept){
	    std::stringstream ss;
	    ss << "ExecData " << it->get_json().dump(4) << " rejected counter " << count.top().get_json().dump(4);
	    recoverable_error(ss.str());
	  }
	}
	count.pop();
      }
      for(auto rit = reinsert.rbegin(); rit != reinsert.rend(); rit++){
	verboseStream << "Warning: Reinserting " << rit->get_json().dump() << " onto counter stack (Tau likely provided this event out of order)" << std::endl;
	count.push(*rit); //reinsert in reverse order (oldest first)
      }
    }

    //Add the now complete event to the map
    m_execDataMap[event.fid()].push_back(it);

    //Check if the event has a correlation ID counter, if so try to match it to an outstanding unmatched
    //event with a correlation ID
    checkAndMatchCorrelationID(it);

    return EventError::OK;
  }//if EXIT

  return EventError::UnknownEvent;
}

EventError ADEvent::addComm(const Event_t& event) {
  if (m_eventType == nullptr) {
    std::cerr << "Uninitialized eventType\n";
    return EventError::UnknownEvent;
  }
  int eid = static_cast<int>(event.eid());

  bool is_send(false), is_recv(false);

  if(eid == m_eidx_comm_send){
    is_send = true;
  }else if(eid == m_eidx_comm_recv){
    is_recv = true;
  }else{
    //Event might be an unknown type *or* the m_eidx* members have not yet been set
    auto it = m_eventType->find(eid);
    if(it == m_eventType->end()){ //event index is not in the map??
      std::cerr << "Unknown event in eventType: " << eid << std::endl;
      return EventError::UnknownEvent;
    }
    if(m_eidx_comm_send == -1 && it->second == "SEND"){
      m_eidx_comm_send = eid;
      is_send = true;
    }else if(m_eidx_comm_recv == -1 && it->second == "RECV"){
      m_eidx_comm_recv = eid;
      is_recv = true;
    }
  }

  if (is_send || is_recv) {
    CommStack_t& cs = m_commStack[event.pid()][event.rid()][event.tid()];
    cs.push(CommData_t(event, is_send ? "SEND" : "RECV"));
    return EventError::OK;
  }

  return EventError::UnknownEvent;
}

EventError ADEvent::addCounter(const Event_t& event){
  if (m_counterMap == nullptr)
    return EventError::UnknownEvent;

  //Maybe add a filter to only include select counters

  int eid = event.counter_id();
  auto it = m_counterMap->find(eid);
  if (it == m_counterMap->end())
    return EventError::UnknownEvent;

  const std::string &counterName = it->second;
  CounterStack_t &cs = m_counterStack[event.pid()][event.rid()][event.tid()];
  cs.push(CounterData_t(event, counterName));

  if(enableVerboseLogging() && counterName == "Correlation ID"){
    verboseStream << "Found correlation ID " << event.counter_value() << std::endl;
  }
  
  return EventError::OK;
}


template <typename T>
static unsigned long nested_map_size(const T& m) {
  size_t n_elements = 0;

  for (auto it: m)
    for (auto itt: it.second)
      for (auto ittt: itt.second)
	n_elements += ittt.second.size();

  return n_elements;
}

CallListMap_p_t* ADEvent::trimCallList(int n_keep_thread) {
  //Remove completed entries from the call list
  CallListMap_p_t* cpListMap = new CallListMap_p_t;
  for (auto& it_p : m_callList) {
    for (auto& it_r : it_p.second) {
      for (auto& it_t: it_r.second) {
	CallList_t& cl = it_t.second;

	//Are we keeping all events for this thread?
	if(n_keep_thread >= cl.size())
	  continue;
	CallList_t cpList;

	auto it = cl.begin();
	auto one_past_last = std::prev(cl.end(),n_keep_thread);

	while (it != one_past_last) {
	  // it = (it->get_runtime() < MAX_RUNTIME) ? cl.erase(it): ++it;
	  if (it->can_delete() && it->get_runtime()) {
	    //Add copy of completed event to output
	    cpList.push_back(*it);
	    //Remove completed event from map of event index string to call list
	    m_callIDMap.erase(it->get_id());
	    //Remove completed event from call list
	    it = cl.erase(it);
	  }
	  else {
	    it++;
	  }
	}
	if (cpList.size())
	  (*cpListMap)[it_p.first][it_r.first][it_t.first] = std::move(cpList); //save a copy
      }
    }
  }
  m_execDataMap.clear();
  return cpListMap;
}


void ADEvent::purgeCallList(int n_keep_thread) {
  //Remove completed entries from the call list
  for (auto& it_p : m_callList) {
    for (auto& it_r : it_p.second) {
      for (auto& it_t: it_r.second) {
	CallList_t& cl = it_t.second;

	//Are we keeping all events for this thread?
	if(n_keep_thread >= cl.size())
	  continue;

	auto it = cl.begin();
	auto one_past_last = std::prev(cl.end(),n_keep_thread);

	while (it != one_past_last) {
	  if (it->can_delete() && it->get_runtime()) {
	    //Remove completed event from map of event index string to call list
	    m_callIDMap.erase(it->get_id());
	    //Remove completed event from call list
	    it = cl.erase(it);
	  }
	  else {
	    it++;
	  }
	}
      }
    }
  }
  m_execDataMap.clear();
}

size_t ADEvent::getCallListSize() const{
  size_t out = 0;
  for (auto& it_p : m_callList) {
    for (auto& it_r : it_p.second) {
      for (auto& it_t: it_r.second) {
	out += it_t.second.size();
      }
    }
  }
  return out;
}


CallListIterator_t ADEvent::getCallData(const eventID &event_id) const{
  auto it = m_callIDMap.find(event_id);
  if(it == m_callIDMap.end()) throw std::runtime_error("Call " + event_id.toString() + " not present in map");
  else return it->second;
}

std::pair<CallListIterator_t, CallListIterator_t> ADEvent::getCallWindowStartEnd(const eventID &event_id, const int win_size) const{
  CallListIterator_t it = getCallData(event_id);
  CallList_t* cl = getElemPRT(it->get_pid(), it->get_rid(), it->get_tid(), const_cast<CallListMap_p_t&>(m_callList)); //need non-const iterator
  if(cl == nullptr)  throw std::runtime_error("ADEvent::getCallWindowStartEnd event has unknown pid/rid/tid");

  CallListIterator_t beg = cl->begin();
  CallListIterator_t end = cl->end();

  CallListIterator_t prev_n = it;
  for (unsigned int i = 0; i < win_size && prev_n != beg; i++)
    prev_n = std::prev(prev_n);

  CallListIterator_t next_n = it;
  for (unsigned int i = 0; i < win_size + 1 && next_n != end; i++)
    next_n = std::next(next_n);

  return {prev_n, next_n};
}




void ADEvent::show_status(bool verbose) const {
  size_t n_comm_remained = nested_map_size<CommStackMap_p_t>(m_commStack);
  size_t n_exec_remained = nested_map_size<CallStackMap_p_t>(m_callStack);
  size_t n_exec = nested_map_size<CallListMap_p_t>(m_callList);

  std::cout << "***** EVENT STATUS *****" << std::endl;
  std::cout << "Num. comm (remained): " << n_comm_remained << std::endl;
  std::cout << "Num. exec (remained): " << n_exec_remained << std::endl;
  std::cout << "Num. exec (total)   : " << n_exec << std::endl;
  if (verbose) {
    for (auto it : m_execDataMap) {
      std::cout << "Function: " << m_funcMap->find(it.first)->second << std::endl;
      for (auto itt: it.second) {
	std::cout << itt->get_json(true) << std::endl;
      }
    }
  }
  std::cout << "***** END   STATUS *****" << std::endl;
}
