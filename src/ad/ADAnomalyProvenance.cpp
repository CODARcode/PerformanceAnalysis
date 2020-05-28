#include <chimbuko/ad/ADAnomalyProvenance.hpp>
#include <chimbuko/verbose.hpp>

using namespace chimbuko;


ADAnomalyProvenance::ADAnomalyProvenance(const ExecData_t &call, const ADEvent &event_man, const ParamInterface &func_stats,
					 const ADCounter &counters): m_call(call){
  //Get stack information
  m_callstack.push_back(call.get_funcname());
  std::string parent = call.get_parent();
  while(parent != "root"){
    auto call_it = event_man.getCallData(parent);
    m_callstack.push_back(call_it->get_funcname());
    parent = call_it->get_parent();
  }

  //Get the function statistics
  m_func_stats = func_stats.get_function_stats(call.get_fid()).get_json();

  //Get the counters that appeared during the execution window on this p/r/t
  std::list<CounterDataListIterator_t> win_count = counters.getCountersInWindow(call.get_pid(), call.get_rid(), call.get_tid(),
										call.get_entry(), call.get_exit());
  m_counters.resize(win_count.size());
  size_t i=0;
  for(auto &e : win_count){
    m_counters[i++] = e->get_json();
  }
  
  //Verbose output
  if(Verbose::on()){
    std::cout << "Anomaly:" << this->get_json() << std::endl;
  }
}


nlohmann::json ADAnomalyProvenance::get_json() const{
  return {
    {"pid", m_call.get_pid()},
      {"rid", m_call.get_rid()},
	{"tid", m_call.get_tid()},
	  {"event_id", m_call.get_id()},
	    {"fid", m_call.get_fid()},
	      {"func", m_call.get_funcname()},
		{"entry", m_call.get_entry()},
		  {"exit", m_call.get_exit()},
		    {"runtime_total", m_call.get_runtime()},
		      {"runtime_exclusive", m_call.get_exclusive()},
			{"call_stack", m_callstack},
			  {"func_stats", m_func_stats},
			    {"counter_events", m_counters}
  };
}
