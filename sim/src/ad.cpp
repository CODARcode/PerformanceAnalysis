#include<chimbuko/util/Anomalies.hpp>
#include<chimbuko/ad/ADLocalFuncStatistics.hpp>
#include<chimbuko/ad/ADLocalCounterStatistics.hpp>
#include<chimbuko/ad/ADNormalEventProvenance.hpp>
#include<chimbuko/ad/ADAnomalyProvenance.hpp>
#include<chimbuko/util/error.hpp>

#include<sim/ad.hpp>
#include<sim/provdb.hpp>
#include<sim/event_id_map.hpp>
#include<sim/id_map.hpp>
#include<sim/pserver.hpp>
#include<sim/ad_params.hpp>

using namespace chimbuko;
using namespace chimbuko_sim;

void ADsim::init(int window_size, int pid, int rid, unsigned long program_start, unsigned long step_freq){
  m_window_size = window_size;
  m_pid = pid;
  m_rid = rid;
  m_pdb_client.reset(new ADProvenanceDBclient(rid));
  m_pdb_client->setEnableHandshake(false);
  m_pdb_client->connect(getProvDB().getAddr(), getProvDB().getNshards());
  m_program_start = program_start;
  m_step_freq = step_freq;
  m_largest_step = 0;
}

CallListIterator_t ADsim::addExec(const int thread,
				  const std::string &func_name,
				  unsigned long start,
				  unsigned long runtime,
				  bool is_anomaly,
				  double outlier_score){
  if(start < m_program_start) fatal_error("Exec start is before program start");

  auto it = funcIdxMap().find(func_name);
  if(it == funcIdxMap().end()) assert(0);
  unsigned long func_id = it->second;
    
  unsigned long exit = start + runtime;
  unsigned long step = ( exit - m_program_start ) / m_step_freq;

  long event_idx = m_step_execs[step].size();
    
  m_largest_step = std::max(m_largest_step, step);

  eventID id(m_rid, step, event_idx);

  ExecData_t exec(id, m_pid, m_rid, thread, func_id, func_name, start, start+runtime);
  exec.set_label( is_anomaly ? -1 : 1 );

  if(is_anomaly) exec.set_outlier_score(outlier_score);

  CallListIterator_t exec_it = m_all_execs[thread].insert(m_all_execs[thread].end(), exec);
  m_eventid_map[id] = exec_it;

  m_step_execs[step].push_back(exec_it);
  return exec_it;
}

void ADsim::attachCounter(const std::string &counter_name,
			  unsigned long value,
			  long t_delta,
			  CallListIterator_t to){
  unsigned long cid = getCidxManager().getIndex(counter_name); //auto handles new counters
    
  long ts = to->get_entry() + t_delta;
  assert(ts <= to->get_exit());

  CounterData_t cdata(m_pid, m_rid, to->get_tid(), cid, counter_name, value, ts);
  to->add_counter(cdata);
}

void ADsim::attachComm(CommType type,
		       unsigned long partner_rank,
		       unsigned long bytes,
		       long t_delta,
		       CallListIterator_t to){
    
  static unsigned long tag = 0;
  long ts = to->get_entry() + t_delta;
  assert(ts <= to->get_exit());
    
  CommData_t comm(m_pid, m_rid, to->get_tid(), partner_rank, bytes, tag++, ts, type == CommType::Send ? "SEND" : "RECV");
  to->add_message(comm);
}
   
void ADsim::registerGPUthread(const int tid){   
  std::vector<MetaData_t> d = {
    MetaData_t(m_pid, m_rid, tid, "CUDA Context", "0"),
    MetaData_t(m_pid, m_rid, tid, "CUDA Stream", "0"),
    MetaData_t(m_pid, m_rid, tid, "CUDA Device", "0") };
  m_metadata.addData(d);
}
    
void ADsim::bindCPUparentGPUkernel(CallListIterator_t cpu_parent, CallListIterator_t gpu_kern){
  assert(m_metadata.isGPUthread(gpu_kern->get_tid()));
  assert(!m_metadata.isGPUthread(cpu_parent->get_tid()));

  static std::string cname = "Correlation ID";
  static unsigned long cidx_cnt = 0;
  unsigned long cidx = cidx_cnt++;

  attachCounter(cname, cidx, 0, cpu_parent);
  attachCounter(cname, cidx, 0, gpu_kern);

  gpu_kern->set_GPU_correlationID_partner(cpu_parent->get_id());
  cpu_parent->set_GPU_correlationID_partner(gpu_kern->get_id());
}

void ADsim::bindParentChild(CallListIterator_t parent, CallListIterator_t child){
  parent->inc_n_children();
  parent->update_exclusive(child->get_runtime());
  //Tell the child who it's parent is
  child->set_parent(parent->get_id());
}


void ADsim::step(const unsigned long step){
  unsigned long step_start_time = m_program_start + step * m_step_freq;
  unsigned long step_end_time = step_start_time + m_step_freq;

  //Collect events in the timestep
  auto mse = m_step_execs.find(step);
  if(mse == m_step_execs.end()){
    std::cout << "Step " << step << " contains no executions" << std::endl;
    return;
  }
  std::list<CallListIterator_t> const& this_step_execs = mse->second;

  //Collect outliers and 1 normal event/func into Anomalies object
  //and collect the map of function index to execs
  Anomalies anom;
  std::unordered_map<unsigned long, std::vector<CallListIterator_t> > this_step_func_execs;
  ADCounter counters;
  counters.linkCounterMap(getCidxManager().getCounterMap());

  int nanom=0, nnorm=0;
  for(const auto &exec_it : this_step_execs){
    if(exec_it->get_label() == -1){ anom.insert(exec_it, Anomalies::EventType::Outlier); nanom++; }
    else if(anom.nFuncEvents(exec_it->get_fid(), Anomalies::EventType::Normal) == 0){ anom.insert(exec_it, Anomalies::EventType::Normal); nnorm++; }

    this_step_func_execs[exec_it->get_fid()].push_back(exec_it);
    
    const std::deque<CounterData_t>& ect = exec_it->get_counters();
    for(const CounterData_t &c : ect)
      counters.addCounter(c);
  }

  //TODO: Normal events need to be processed correctly

  std::cout << "Step " << step << " rank " << m_rid << " " << nanom << " anomalies and " << nnorm << " normal events" << std::endl;

  //Extract provenance data and send to the provDB
  if(nanom > 0){
    std::vector<nlohmann::json> normal_events;
    std::vector<nlohmann::json> anomalous_events;

    eventIDmap evmap(m_all_execs, m_eventid_map);
      
    ADAnomalyProvenance::getProvenanceEntries(anomalous_events, normal_events, m_normal_events,
					      anom, step, step_start_time, step_end_time, m_window_size,
					      globalParams(), evmap, counters, m_metadata);
    //Send to provdb
    if(anomalous_events.size() > 0)
      m_pdb_client->sendMultipleData(anomalous_events, ProvenanceDataType::AnomalyData); 
    if(normal_events.size() > 0)
      m_pdb_client->sendMultipleData(normal_events, ProvenanceDataType::NormalExecData);   
  }
    

  //Update the pserver
  {
    ADLocalCounterStatistics count_stats(m_pid, step, nullptr);
    count_stats.gatherStatistics(counters.getCountersByIndex());
    getPserver().addCounterData(count_stats);

    ADLocalFuncStatistics prof_stats(m_pid, m_rid, step);
    prof_stats.gatherStatistics(&this_step_func_execs); //takes map of fid -> vector of ExecData iterators
    prof_stats.gatherAnomalies(anom);
    getPserver().addAnomalyData(prof_stats);
  }
}


size_t ADsim::nStepExecs(unsigned long step) const{
  auto mse = m_step_execs.find(step);
  if(mse == m_step_execs.end()){
    return 0;
  }else return mse->second.size();
}
  
