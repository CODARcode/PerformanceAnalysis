#include <chimbuko/ad/ADAnomalyProvenance.hpp>
#include <chimbuko/verbose.hpp>

using namespace chimbuko;

inline nlohmann::json getCallStackEntry(const ExecData_t &call){
  return { {"fid",call.get_fid()}, {"func",call.get_funcname()}, {"entry_time",call.get_entry()} };
};

void ADAnomalyProvenance::getStackInformation(const ExecData_t &call, const ADEvent &event_man){
  //Get stack information
  m_callstack.push_back(getCallStackEntry(call));
  std::string parent = call.get_parent();
  while(parent != "root"){
    auto call_it = event_man.getCallData(parent);
    m_callstack.push_back(getCallStackEntry(*call_it));
    parent = call_it->get_parent();
  }
}

void ADAnomalyProvenance::getWindowCounters(const ExecData_t &call){
  //Get the counters that appeared during the execution window on this p/r/t
  const std::deque<CounterData_t> &win_count = call.get_counters();

  m_counters.resize(win_count.size());
  size_t i=0;
  for(auto &e : win_count){
    m_counters[i++] = e.get_json();
  }
}

void ADAnomalyProvenance::getGPUeventInfo(const ExecData_t &call, const ADEvent &event_man, const ADMetadataParser &metadata){
 //Determine if it is a GPU event, and if so get the context
  m_is_gpu_event = metadata.isGPUthread(call.get_tid());
  if(m_is_gpu_event){
    VERBOSE(std::cout << "Call is a GPU event" << std::endl);
    m_gpu_location = metadata.getGPUthreadInfo(call.get_tid()).get_json();

    //Find out information about the CPU event that spawned it
    if(call.has_GPU_correlationID_partner()){
      VERBOSE(std::cout << "Call has a GPU correlation ID partner: " <<  call.get_GPU_correlationID_partner() << std::endl);
      std::string gpu_event_parent = call.get_GPU_correlationID_partner();
      CallListIterator_t pit = event_man.getCallData(gpu_event_parent);
      m_gpu_event_parent_info["event_id"] = gpu_event_parent;
      m_gpu_event_parent_info["tid"] = pit->get_tid();

      //Generate the parent stack
      nlohmann::json gpu_event_parent_stack = nlohmann::json::array();
      gpu_event_parent_stack.push_back(getCallStackEntry(*pit));

      std::string parent = pit->get_parent();
      while(parent != "root"){
	auto call_it = event_man.getCallData(parent);
	gpu_event_parent_stack.push_back(getCallStackEntry(*call_it));
	parent = call_it->get_parent();
      }
      m_gpu_event_parent_info["call_stack"] = std::move(gpu_event_parent_stack);
    }
  }
}

void ADAnomalyProvenance::getExecutionWindow(const ExecData_t &call,
					     const ADEvent &event_man,
					     const int window_size){
  std::pair<CallListIterator_t, CallListIterator_t> win = event_man.getCallWindowStartEnd(call.get_id(), window_size);
  m_exec_window["exec_window"] = nlohmann::json::array();
  m_exec_window["comm_window"] = nlohmann::json::array();
  for(auto it = win.first; it != win.second; it++){
    m_exec_window["exec_window"].push_back( { 
	{"fid", it->get_fid()}, 
	  {"func", it->get_funcname() }, 
	    {"event_id", it->get_id()}, 
	      {"entry", it->get_entry()},
		{"exit", it->get_exit() == 0 ? -1 : it->get_exit()}
      });
    for(CommData_t const &comm : it->get_messages())
      m_exec_window["comm_window"].push_back(comm.get_json());    
  }
}




ADAnomalyProvenance::ADAnomalyProvenance(const ExecData_t &call, const ADEvent &event_man, const ParamInterface &func_stats,
					 const ADCounter &counters, const ADMetadataParser &metadata, const int window_size,
					 const int io_step, 
					 const unsigned long io_step_tstart, const unsigned long io_step_tend): 
  m_call(call), m_is_gpu_event(false), 
  m_io_step(io_step), m_io_step_tstart(io_step_tstart), m_io_step_tend(io_step_tend)
{
  getStackInformation(call, event_man); //get call stack
  m_func_stats = func_stats.get_function_stats(call.get_fid()).get_json();   //Get the function statistics
  getWindowCounters(call); //counters in window 
  getGPUeventInfo(call, event_man, metadata); //info of GPU event (if applicable)
  getExecutionWindow(call, event_man, window_size);
 
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
			      {"counter_events", m_counters},
				{"is_gpu_event", m_is_gpu_event},
				  {"gpu_location", m_is_gpu_event ? m_gpu_location : nlohmann::json() },
				    {"gpu_parent", m_is_gpu_event ? m_gpu_event_parent_info : nlohmann::json() },
				      {"event_window", m_exec_window},
					{"io_step", m_io_step},
					  {"io_step_tstart", m_io_step_tstart},
					    {"io_step_tend", m_io_step_tend}
					    

  };
}
