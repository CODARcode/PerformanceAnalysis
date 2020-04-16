#include <chimbuko/ad/ADAnomalyProvenance.hpp>
#include <chimbuko/verbose.hpp>

using namespace chimbuko;


ADAnomalyProvenance::ADAnomalyProvenance(const ExecData_t &call, const ADEvent &event_man, const ParamInterface &func_stats): m_call(call){
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
    
  if(Verbose::on()){
    std::cout << "Anomaly:" << this->get_json() << std::endl;
  }
}


nlohmann::json ADAnomalyProvenance::get_json() const{
  return {
    {"pid", m_call.get_pid()},
      {"rid", m_call.get_rid()},
	{"tid", m_call.get_tid()},
	  {"func", m_call.get_id()},
	    {"entry", m_call.get_entry()},
	      {"exit", m_call.get_exit()},
		{"runtime_total", m_call.get_runtime()},
		  {"runtime_exclusive", m_call.get_exclusive()},
		    {"call_stack", m_callstack},
		      {"func_stats", m_func_stats}
	};
}
