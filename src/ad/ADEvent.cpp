#include "chimbuko/verbose.hpp"
#include "chimbuko/ad/ADEvent.hpp"
#include "chimbuko/util/map.hpp"
#include <iostream>

using namespace chimbuko;

ADEvent::ADEvent(bool verbose) 
  : m_funcMap(nullptr), m_eventType(nullptr), m_counterMap(nullptr), m_verbose(verbose) 
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


void ADEvent::checkAndMatchCorrelationID(CallListIterator_t it){
  //Check if the event has a correlation ID counter, if so try to match it to an outstanding unmatched
  //event with a correlation ID
  bool found_cid = false;
  for(auto const &c : it->get_counters()){
    if(c.get_countername() == "Correlation ID"){
      if(found_cid) throw std::runtime_error("Event has more than 1 Correlation ID event associated with it?!?");
      found_cid = true;

      unsigned long cid = c.get_value();
	
      //Does a partner already exist?
      auto m = m_unmatchedCorrelationID.find(cid);
      if(m != m_unmatchedCorrelationID.end()){
	std::string current_event_id = it->get_id();
	std::string partner_event_id = m->second->get_id();
	it->set_GPU_correlationID_partner(partner_event_id);
	m->second->set_GPU_correlationID_partner(current_event_id);
	m->second->can_delete(true); //allow partner event to be purged at end of io step
	VERBOSE(std::cout << "Found partner event " << current_event_id << " to previous unmatched event " << partner_event_id << " with correlation ID " << cid << std::endl);
      }else{
	//Ensure the event can't be deleted and put it in the map of unmatched events
	it->can_delete(false);
	m_unmatchedCorrelationID[cid] = it;
	VERBOSE(std::cout << "Found as-yet unpartnered event with correlation ID " << cid << std::endl);
      }
    }
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
  if (m_eventType == nullptr) {
    std::cerr << "Uninitialized eventType\n";
    return EventError::UnknownEvent;
  }

  std::string eventType;
  int eid = static_cast<int>(event.eid());
  if (m_eventType->count(eid) == 0) {
    std::cerr << "Unknown event in eventType: " << eid << std::endl;
    return EventError::UnknownEvent;
  }

  if (m_funcMap == nullptr) {
    std::cerr << "Uninitialized function map\n";
    return EventError::UnknownEvent;
  }
    
  if (m_funcMap->count(event.fid()) == 0) {
    std::cerr << "Unknown function event\n";
    return EventError::UnknownFunc;
  }

  // if ( m_funcMap->find(event.fid())->second.find("pthread") != std::string::npos ) {
  // //    std::cerr << "Skip: " << m_funcMap->find(event.fid())->second << std::endl;
  //     return EventError::OK;
  // }

  eventType = m_eventType->find(eid)->second;

  //if (m_verbose)
  //    std::cerr << event << ": "
  //                    << m_eventType->find(event.eid())->second << ": "
  //                    << m_funcMap->find(event.fid())->second << std::endl;
  //return EventError::OK;


  if (eventType.compare("ENTRY") == 0){
    //Create a new ExecData_t object with function entry information and push onto the call list
    CallList_t& cl = m_callList[event.pid()][event.rid()][event.tid()];

    CallListIterator_t it = cl.emplace(cl.end(),event);

    //Push the function call onto the stack, let the previous top function know it has a child and tell the new function call who its parent is
    CallStack_t& cs = m_callStack[event.pid()][event.rid()][event.tid()];
    if (cs.size()) {
      it->set_parent(cs.top()->get_id());
      cs.top()->inc_n_children();
    }
    it->set_funcname(m_funcMap->find(event.fid())->second);
    cs.push(it);

    //Add the new call to the map of call index string
    m_callIDMap[it->get_id()] = it;
    
    return EventError::OK;
  }else if (eventType.compare("EXIT") == 0){
    CallStack_t& cs = m_callStack[event.pid()][event.rid()][event.tid()];
    if (cs.size() == 0) { //Expect to have at least one entry; that created when the ENTRY was encountered
      std::cerr << "\n***** Empty call stack! *****\n" << std::endl;
      std::cerr << event.get_json().dump() << std::endl
		<< m_eventType->find(event.eid())->second << ": "
		<< m_funcMap->find(event.fid())->second << std::endl;
      return EventError::EmptyCallStack;
    }

    //Get the function call object at the top of the stack and add the exit event
    CallListIterator_t& it = cs.top();
    if (!it->update_exit(event)) {
      std::cerr << "\n***** Invalid EXIT event! *****\n" << std::endl;
      std::cerr << event.get_json().dump() << std::endl
		<< m_eventType->find(event.eid())->second << ": "
		<< m_funcMap->find(event.fid())->second << std::endl;
      std::cerr << it->get_json().dump() << std::endl;
      // while (!cs.empty()) {
      //     std::cerr << *cs.top() << std::endl;
      //     cs.pop();
      // }            
      return EventError::CallStackViolation;
    }
    //Remove the object from the stack (it still lives in the CallList)
    cs.pop(); //I'm surprised this doesn't invalidate the iterator
    //Tell the former parent function to subtract the child runtime from its exclusive runtime 
    if (!cs.empty()) {
      cs.top()->update_exclusive(it->get_runtime());
    }

    //Associate all comms events on the stack with the call object
    CommStack_t& comm = m_commStack[event.pid()][event.rid()][event.tid()];
    while (!comm.empty()) {
      if (!it->add_message(comm.top(), ListEnd::Front))    //stack access is reverse time order!
	break;
      comm.pop();
    }

    //Associate all counter events on the stack with the call object
    CounterStack_t& count = m_counterStack[event.pid()][event.rid()][event.tid()];
    while (!count.empty()) {
      if (!it->add_counter(count.top(), ListEnd::Front))
	break;
      count.pop();
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
  if (m_eventType == nullptr)
    return EventError::UnknownEvent;

  int eid = static_cast<int>(event.eid());
  if (m_eventType->count(eid) == 0)
    return EventError::UnknownEvent;

  std::string eventType = m_eventType->find(eid)->second;

  if (eventType.compare("SEND") == 0 || eventType.compare("RECV") == 0) {
    CommStack_t& cs = m_commStack[event.pid()][event.rid()][event.tid()];
    cs.push(CommData_t(event, eventType));
  }
  else {
    // std::cout << eventType << std::endl;
    return EventError::UnknownEvent;
  }
    
  return EventError::OK;
}

EventError ADEvent::addCounter(const Event_t& event){
  if (m_counterMap == nullptr)
    return EventError::UnknownEvent;

  //Maybe add a filter to only include select counters
  
  int eid = event.counter_id();
  auto it = m_counterMap->find(eid);
  if (it == m_counterMap->end())
    return EventError::UnknownEvent;
  
  std::string counterName = it->second;
  CounterStack_t &cs = m_counterStack[event.pid()][event.rid()][event.tid()];
  cs.push(CounterData_t(event, counterName));
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

CallListMap_p_t* ADEvent::trimCallList() {
  //Remove completed entries from the call list
  CallListMap_p_t* cpListMap = new CallListMap_p_t;
  for (auto& it_p : m_callList) {
    for (auto& it_r : it_p.second) {
      for (auto& it_t: it_r.second) {
	CallList_t& cl = it_t.second;
	CallList_t cpList;

	auto it = cl.begin();
	while (it != cl.end()) {
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


CallListIterator_t ADEvent::getCallData(const std::string &event_id) const{
  auto it = m_callIDMap.find(event_id);
  if(it == m_callIDMap.end()) throw std::runtime_error("Call " + event_id + " not present in map");
  else return it->second;
}

std::pair<CallListIterator_t, CallListIterator_t> ADEvent::getCallWindowStartEnd(const std::string &event_id, const int win_size) const{
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
