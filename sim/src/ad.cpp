#include<chimbuko/util/Anomalies.hpp>
#include<chimbuko/ad/ADLocalFuncStatistics.hpp>
#include<chimbuko/ad/ADLocalCounterStatistics.hpp>
#include<chimbuko/ad/ADNormalEventProvenance.hpp>
#include<chimbuko/ad/ADAnomalyProvenance.hpp>

#include<sim/ad.hpp>
#include<sim/provdb.hpp>
#include<sim/event_id_map.hpp>
#include<sim/id_map.hpp>
#include<sim/pserver.hpp>
#include<sim/ad_params.hpp>

using namespace chimbuko;
using namespace chimbuko_sim;

void ADsim::init(int window_size, int pid, int rid){
  m_step = 0;
  m_window_size = window_size;
  m_pid = pid;
  m_rid = rid;
  m_pdb_client.reset(new ADProvenanceDBclient(rid));
  m_pdb_client->setEnableHandshake(false);
  m_pdb_client->connect(getProvDB().getAddr(), getProvDB().getNshards());
  m_counters.linkCounterMap(getCidxManager().getCounterMap());
}

CallListIterator_t ADsim::addExec(const int thread,
				  const std::string &func_name,
				  unsigned long start,
				  unsigned long runtime,
				  bool is_anomaly,
				  double outlier_score){
  auto it = funcIdxMap().find(func_name);
  if(it == funcIdxMap().end()) assert(0);
  unsigned long func_id = it->second;
    
  static long event_idx_s = 0;
  long event_idx = event_idx_s++;
    
  eventID id(m_rid, m_step, event_idx);

  ExecData_t exec(id, m_pid, m_rid, thread, func_id, func_name, start, start+runtime);
  exec.set_label( is_anomaly ? -1 : 1 );

  if(is_anomaly) exec.set_outlier_score(outlier_score);

  CallListIterator_t exec_it = m_all_execs[thread].insert(m_all_execs[thread].end(), exec);
  m_eventid_map[id] = exec_it;

  m_step_exec_its.push_back(exec_it);
  m_step_func_exec_its[func_id].push_back(exec_it);

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
  m_counters.addCounter(cdata);
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

void ADsim::beginStep(const unsigned long step_start_time){
  m_step_start_time = step_start_time;
}

void ADsim::endStep(const unsigned long step_end_time){
  //Collect outliers and 1 normal event/func into Anomalies object
  Anomalies anom;

  int nanom=0, nnorm=0;
  for(const auto &exec_it : m_step_exec_its){
    if(exec_it->get_label() == -1){ anom.insert(exec_it, Anomalies::EventType::Outlier); nanom++; }
    else if(anom.nFuncEvents(exec_it->get_fid(), Anomalies::EventType::Normal) == 0){ anom.insert(exec_it, Anomalies::EventType::Normal); nnorm++; }
  }

  std::cout << "Rank " << m_rid << " " << nanom << " anomalies and " << nnorm << " normal events" << std::endl;
  std::cout << "TEST " << anom.nEvents(Anomalies::EventType::Outlier) << " " << anom.nEvents(Anomalies::EventType::Normal) << std::endl;

  //Extract provenance data and send to the provDB
  if(nanom > 0){
    std::vector<nlohmann::json> normal_events;
    std::vector<nlohmann::json> anomalous_events;

    eventIDmap evmap(m_all_execs, m_eventid_map);
      
    ADAnomalyProvenance::getProvenanceEntries(anomalous_events, normal_events, m_normal_events,
					      anom, m_step, m_step_start_time, step_end_time, m_window_size,
					      globalParams(), evmap, m_counters, m_metadata);
    //Send to provdb
    if(anomalous_events.size() > 0)
      m_pdb_client->sendMultipleData(anomalous_events, ProvenanceDataType::AnomalyData); 
    if(normal_events.size() > 0)
      m_pdb_client->sendMultipleData(normal_events, ProvenanceDataType::NormalExecData);   
  }
    

  //Update the pserver
  {
    ADLocalCounterStatistics count_stats(m_pid, m_step, nullptr);
    count_stats.gatherStatistics(m_counters.getCountersByIndex());
    getPserver().addCounterData(count_stats);

    ADLocalFuncStatistics prof_stats(m_pid, m_rid, m_step);
    prof_stats.gatherStatistics(&m_step_func_exec_its); //takes map of fid -> vector of ExecData iterators
    prof_stats.gatherAnomalies(anom);
    getPserver().addAnomalyData(prof_stats);
  }

  m_step_exec_its.clear();
  m_step_func_exec_its.clear();
  delete m_counters.flushCounters();

  m_step++;
}
