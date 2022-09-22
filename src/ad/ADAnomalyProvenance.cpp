#include <chimbuko/ad/ADAnomalyProvenance.hpp>
#include <chimbuko/verbose.hpp>
#include <chimbuko/util/error.hpp>
#include <chimbuko/util/environment.hpp>

using namespace chimbuko;

ADAnomalyProvenance::ADAnomalyProvenance(const ADEventIDmap &event_man): m_perf(nullptr), m_event_man(&event_man), m_monitoring(nullptr), m_metadata(nullptr), m_algo_params(nullptr), m_window_size(5), m_min_anom_time(0){}


inline nlohmann::json getCallStackEntry(const ExecData_t &call){
  return { {"fid",call.get_fid()}, {"func",call.get_funcname()}, {"entry",call.get_entry()}, {"exit", call.get_exit()}, {"event_id", call.get_id().toString()}, {"is_anomaly", call.get_label() == -1} };
};

void ADAnomalyProvenance::getStackInformation(nlohmann::json &into, const ExecData_t &call) const{
  //Get stack information
  nlohmann::json &callstack = (into["call_stack"] = nlohmann::json::array());

  callstack.push_back(getCallStackEntry(call));
  eventID parent = call.get_parent();
  while(parent != eventID::root()){
    CallListIterator_t call_it;
    try{
      call_it = m_event_man->getCallData(parent);
    }catch(const std::exception &e){
      recoverable_error("Could not find parent " + parent.toString() + " in call list due to : " + e.what());
      break;
    }

    callstack.push_back(getCallStackEntry(*call_it));
    parent = call_it->get_parent();
  }
}

void ADAnomalyProvenance::getWindowCounters(nlohmann::json &into, const ExecData_t &call) const{
  //Get the counters that appeared during the execution window on this p/r/t
  const std::deque<CounterData_t> &win_count = call.get_counters();

  nlohmann::json &counters = (into["counter_events"] = nlohmann::json::array());
  for(auto &e : win_count){
    counters.push_back(e.get_json());
  }
}


void ADAnomalyProvenance::getGPUeventInfo(nlohmann::json &into, const ExecData_t &call) const{
  if(m_metadata == nullptr) return;

  //Initialize to empty
  nlohmann::json &gpu_location = (into["gpu_location"] = nlohmann::json());
  nlohmann::json &gpu_parent = (into["gpu_parent"] = nlohmann::json());
  into["is_gpu_event"] = false;

  //Determine if it is a GPU event, and if so get the context
  if(m_metadata->isGPUthread(call.get_tid())){
    into["is_gpu_event"] = true;

    verboseStream << "Call is a GPU event" << std::endl;
    gpu_location = m_metadata->getGPUthreadInfo(call.get_tid()).get_json();

    //Find out information about the CPU event that spawned it
    if(call.has_GPU_correlationID_partner()){
      //Note a GPU event can only be partnered to one CPU event but a CPU event can be partnered to multiple GPU events
      if(call.n_GPU_correlationID_partner() != 1){
	std::stringstream ss; ss << "GPU event has multiple correlation ID partners?? Event details:" << std::endl << call.get_json(false,true).dump() << std::endl;
	recoverable_error(ss.str());

	gpu_parent = "Chimbuko error: Multiple host parent event correlation IDs found, likely due to trace corruption";
      }else{
	verboseStream << "Call has a GPU correlation ID partner: " <<  call.get_GPU_correlationID_partner(0).toString() << std::endl;

	const eventID &gpu_event_parent = call.get_GPU_correlationID_partner(0);
	gpu_parent["event_id"] = gpu_event_parent.toString();

	//Get the parent event
	CallListIterator_t pit;
	bool got_parent = true;
	try{
	  pit = m_event_man->getCallData(gpu_event_parent);
	}catch(const std::exception &e){
	  recoverable_error("Could not find GPU parent " + gpu_event_parent.toString() + " in call list due to : " + e.what());
	  got_parent = false;
	}

	if(got_parent){
	  gpu_parent["tid"] = pit->get_tid();

	  //Generate the parent stack
	  nlohmann::json gpu_event_parent_stack = nlohmann::json::array();
	  gpu_event_parent_stack.push_back(getCallStackEntry(*pit));

	  eventID parent = pit->get_parent();
	  while(parent != eventID::root()){
	    CallListIterator_t call_it;
	    try{
	      call_it = m_event_man->getCallData(parent);
	    }catch(const std::exception &e){
	      recoverable_error("Could not find GPU stack event parent " + parent.toString() + " in call list due to : " + e.what());
	      break;
	    }
	    gpu_event_parent_stack.push_back(getCallStackEntry(*call_it));
	    parent = call_it->get_parent();
	  }
	  gpu_parent["call_stack"] = std::move(gpu_event_parent_stack);
	}else{
	  //Could not get parent event
	  gpu_parent = "Chimbuko error: Host parent event could not be reached";
	}

      }//have *one* correlation ID partner
    }else{ 
      //No correlation ID recorded
      gpu_parent = "Chimbuko error: Correlation ID of host parent event was not recorded";
    }
  }//is gpu event

}


void ADAnomalyProvenance::getExecutionWindow(nlohmann::json &into, const ExecData_t &call) const{
  nlohmann::json &exec_window_head = into["event_window"];
  nlohmann::json &exec_window = (exec_window_head["exec_window"] = nlohmann::json::array());
  nlohmann::json &comm_window = (exec_window_head["comm_window"] = nlohmann::json::array());

  //Get the window
  std::pair<CallListIterator_t, CallListIterator_t> win;
  try{
    win = m_event_man->getCallWindowStartEnd(call.get_id(), m_window_size);
  }catch(const std::exception &e){
    recoverable_error("Could not get call window for event " + call.get_id().toString());
    return;
  }

  for(auto it = win.first; it != win.second; it++){
    exec_window.push_back( {
	{"fid", it->get_fid()},
	  {"func", it->get_funcname() },
	    {"event_id", it->get_id().toString()},
	      {"entry", it->get_entry()},
		{"exit", it->get_exit()},    //is 0 if function has not exited
		  {"parent_event_id", it->get_parent().toString()},
		    {"is_anomaly", it->get_label() == -1}
      });
    for(CommData_t const &comm : it->get_messages())
      comm_window.push_back(comm.get_json());
  }
}

void ADAnomalyProvenance::getNodeState(nlohmann::json &into) const{
  if(m_monitoring)
    into["node_state"] = m_monitoring->get_json();
}

void ADAnomalyProvenance::getHostname(nlohmann::json &into) const{
  if(m_metadata)
    into["hostname"] = m_metadata->getHostname();
}

void ADAnomalyProvenance::getAlgorithmParams(nlohmann::json &into, const ExecData_t &call) const{
  if(m_algo_params)
    into["algo_params"] = m_algo_params->get_algorithm_params(call.get_fid());
}


nlohmann::json ADAnomalyProvenance::getEventProvenance(const ExecData_t &call,
							const int step,
							const unsigned long first_event_ts,
							const unsigned long last_event_ts) const{
  nlohmann::json out = {
      {"pid", call.get_pid()},
      {"rid", call.get_rid()},
      {"tid", call.get_tid()},
      {"event_id", call.get_id().toString()},
      {"fid", call.get_fid()},
      {"func", call.get_funcname()},
      {"entry", call.get_entry()},
      {"exit", call.get_exit()},
      {"runtime_total", call.get_runtime()},
      {"runtime_exclusive", call.get_exclusive()},
      {"io_step", step},
      {"io_step_tstart", first_event_ts},
      {"io_step_tend", last_event_ts},
      {"outlier_score", call.get_outlier_score() },
      {"outlier_severity", call.get_outlier_severity() },
  };

  getHostname(out);
  getStackInformation(out, call); //get call stack
  getWindowCounters(out, call); //counters in window
  getGPUeventInfo(out, call); //info of GPU event (if applicable)
  getExecutionWindow(out, call);
  getNodeState(out);
  getAlgorithmParams(out,call);
  return out;
}



void ADAnomalyProvenance::getProvenanceEntries(std::vector<nlohmann::json> &anom_event_entries,
						std::vector<nlohmann::json> &normal_event_entries,
						const Anomalies &anomalies,
						const int step,
						const unsigned long first_event_ts,
						const unsigned long last_event_ts){
  constexpr bool do_delete = true;
  constexpr bool add_outstanding = true;

  PerfTimer timer,timer2;

  //Put new normal event provenance into m_normalevent_prov
  timer.start();
  for(auto norm_it : anomalies.allEvents(Anomalies::EventType::Normal)){
    timer2.start();
    m_normalevents.addNormalEvent(norm_it->get_pid(), norm_it->get_rid(), norm_it->get_tid(), norm_it->get_fid(), getEventProvenance(*norm_it, step, first_event_ts, last_event_ts));
    if(m_perf) m_perf->add("ad_extract_send_prov_normalevent_update_per_event_ms", timer2.elapsed_ms());
  }
  if(m_perf) m_perf->add("ad_extract_send_prov_normalevent_update_total_ms", timer.elapsed_ms());

  //Get any outstanding normal events from previous timesteps that we couldn't previously provide
  timer.start();

  normal_event_entries = m_normalevents.getOutstandingRequests(do_delete); //allow deletion of internal copy of events that are returned

  if(m_perf) m_perf->add("ad_extract_send_prov_normalevent_get_outstanding_ms", timer.elapsed_ms());

  //Gather provenance of anomalies and for each one try to obtain a normal execution
  timer.start();
  std::unordered_set<unsigned long> normal_event_fids;

  for(auto anom_it : anomalies.allEvents(Anomalies::EventType::Outlier)){
    timer2.start();
    if(anom_it->get_exclusive() < m_min_anom_time) continue; //skip executions with too short runtimes to avoid filling the database with irrelevant anomalies

    anom_event_entries.push_back(getEventProvenance(*anom_it, step, first_event_ts, last_event_ts));
    if(m_perf) m_perf->add("ad_extract_send_prov_anom_data_generation_per_anom_ms", timer2.elapsed_ms());

    //Get the associated normal event if one has not been recorded for this function on this step
    if(!normal_event_fids.count(anom_it->get_fid())){
      timer2.start();
      //if normal event not available put into the list of outstanding requests and it will be recorded next time a normal event for this function is obtained
      //if normal event is available, delete internal copy within m_normalevent_prov so the normal event isn't added more than once
      auto nev = m_normalevents.getNormalEvent(anom_it->get_pid(), anom_it->get_rid(), anom_it->get_tid(), anom_it->get_fid(), add_outstanding, do_delete);
      if(nev.second) normal_event_entries.push_back(std::move(nev.first));
      
      normal_event_fids.insert(anom_it->get_fid()); //make sure we don't record more than one normal event for this fid
      if(m_perf) m_perf->add("ad_extract_send_prov_normalevent_gather_per_anom_ms", timer2.elapsed_ms());
    }

  }
}






