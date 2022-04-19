#include<chimbuko/util/Anomalies.hpp>
#include<chimbuko/ad/ADLocalFuncStatistics.hpp>
#include<chimbuko/ad/ADLocalCounterStatistics.hpp>
#include<chimbuko/ad/ADNormalEventProvenance.hpp>
#include<chimbuko/ad/ADAnomalyProvenance.hpp>
#include<chimbuko/util/error.hpp>
#include<chimbuko/provdb/setup.hpp>

#include<sim/ad.hpp>
#include<sim/provdb.hpp>
#include<sim/event_id_map.hpp>
#include<sim/id_map.hpp>
#include<sim/pserver.hpp>
#include<sim/ad_params.hpp>

using namespace chimbuko;
using namespace chimbuko_sim;

ADsim::ADsim(ADsim &&r): 
  m_all_execs(std::move(r.m_all_execs)),
  m_eventid_map(std::move(r.m_eventid_map)),
  m_step_execs(std::move(r.m_step_execs)),
  m_entry_step_count(r.m_entry_step_count),
  m_largest_step(r.m_largest_step),
  m_program_start(r.m_program_start),
  m_step_freq(r.m_step_freq),
  m_window_size(r.m_window_size),
  m_pid(r.m_pid),
  m_rid(r.m_rid),
  m_normal_events(std::move(r.m_normal_events)),
  m_metadata(std::move(r.m_metadata)),
#ifdef ENABLE_PROVDB
  m_pdb_client(std::move(r.m_pdb_client)),
#else
  m_prov_io(std::move(r.m_prov_io)),
#endif
  m_outlier(r.m_outlier),
  m_net_client(r.m_net_client)
{
  r.m_outlier = nullptr;
  r.m_net_client = nullptr;
}


void ADsim::init(int window_size, int pid, int rid, unsigned long program_start, unsigned long step_freq, const provDBsetup &pdb_setup){
  m_window_size = window_size;
  m_pid = pid;
  m_rid = rid;
#ifdef ENABLE_PROVDB
  m_pdb_client.reset(new ADProvenanceDBclient(rid));

  if(pdb_setup.use_local){ //use the in-process provDB server
    m_pdb_client->setEnableHandshake(false);
    int shard = rid % getProvDB().getNshards();
    std::string db_name = ProvDBsetup::getShardDBname(shard);
    m_pdb_client->connect(getProvDB().getAddr(), db_name, 0); //connect on provider 0 for in-proc server
  }else{
    m_pdb_client->connectMultiServer(pdb_setup.remote_server_addr_dir, pdb_setup.remote_server_nshards, pdb_setup.remote_server_instances);
  }
#else
  m_prov_io.reset(new ADio(m_pid, m_rid));
  m_prov_io->setDispatcher();
  m_prov_io->setDestructorThreadWaitTime(0); //don't know why we would need a wait
  m_prov_io->setOutputPath(".");
#endif
  m_program_start = program_start;
  m_step_freq = step_freq;
  m_largest_step = 0;

  auto const & p = adAlgorithmParams();
  if(p.algorithm != "none"){    
    ADOutlier::AlgoParams params;
    params.stat = p.stat;
    params.hbos_thres = p.hbos_thres;
    params.glob_thres = p.glob_thres;
    params.sstd_sigma = p.sstd_sigma;

    m_outlier = ADOutlier::set_algorithm(p.algorithm, params);
    getPserver(); //force construction of pserver
    m_net_client = new ADThreadNetClient(true); //use local comms
    m_net_client->connect_ps(m_rid);
    m_outlier->linkNetworkClient(m_net_client);
  }
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

  unsigned long entry_step = ( start - m_program_start ) / m_step_freq;
    
  unsigned long exit = start + runtime;  
  unsigned long exit_step = ( exit - m_program_start ) / m_step_freq;

  m_largest_step = std::max(m_largest_step, exit_step);

  long event_idx = m_entry_step_count[entry_step]++; //events are indexed by entry step as in real execution    
  eventID id(m_rid, entry_step, event_idx);

  ExecData_t exec(id, m_pid, m_rid, thread, func_id, func_name, start, start+runtime);
  if(!m_outlier) exec.set_label( is_anomaly ? -1 : 1 ); //use user tag

  if(is_anomaly) exec.set_outlier_score(outlier_score);

  CallListIterator_t exec_it = m_all_execs[thread].insert(m_all_execs[thread].end(), exec);
  m_eventid_map[id] = exec_it;

  m_step_execs[exit_step].push_back(exec_it);
  return exec_it;
}


void ADsim::updateRuntime(CallListIterator_t exec_it,
			   unsigned long new_runtime){
    
  unsigned long old_exit = exec_it->get_exit();
  unsigned long old_exit_step = ( old_exit - m_program_start ) / m_step_freq;
  
  //Remove the exec from the old step
  bool found = false;
  auto &old_step_list = m_step_execs[old_exit_step];
  for(auto old_step_it = old_step_list.begin(); old_step_it != old_step_list.end(); old_step_it++){
    if( *old_step_it == exec_it ){
      old_step_list.erase(old_step_it);
      found = true;
      break;
    }
  }
  if(!found) fatal_error("Could not find iterator in old step list!");

  unsigned long new_exit = exec_it->get_entry() + new_runtime;  
  unsigned long new_exit_step = ( new_exit - m_program_start ) / m_step_freq;

  exec_it->update_exclusive(old_exit);  //hacky way to reset exit time
  exec_it->update_exit(new_exit);

  m_largest_step = std::max(m_largest_step, new_exit_step);

  //Determine where to insert the entry
  auto &new_step_list = m_step_execs[new_exit_step];
  bool inserted = false;
  
  if(new_step_list.size() > 1){ //if >2 events the execution might be between existing events
    unsigned long prev_exit = (*new_step_list.begin())->get_exit();  
    for(auto new_step_it = std::next(new_step_list.begin(),1); new_step_it != new_step_list.end(); new_step_it++){
      unsigned long cur_exit = (*new_step_it)->get_exit();
      if(new_exit >= prev_exit && new_exit <= cur_exit){ //some ambiguity for 0 runtime functions here
	new_step_list.insert(new_step_it, exec_it);
	inserted = true;
	break;
      }
      prev_exit = cur_exit;
    }
  }
  
  if(!inserted)
    new_step_list.push_back(exec_it);
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
  
  //Collect the map of function index to execs and gather counters
  std::unordered_map<unsigned long, std::vector<CallListIterator_t> > this_step_func_execs; // == ExecData_t
  ADCounter counters;
  counters.linkCounterMap(getCidxManager().getCounterMap());

  for(const auto &exec_it : this_step_execs){
    this_step_func_execs[exec_it->get_fid()].push_back(exec_it);
    
    const std::deque<CounterData_t>& ect = exec_it->get_counters();
    for(const CounterData_t &c : ect)
      counters.addCounter(c);
  }

  //Are we going to run the actual AD algorithm on the data?
  Anomalies anom;

  if(adAlgorithmParams().algorithm != "none"){
    if(!m_outlier) fatal_error("ad_algorithm is set to " + adAlgorithmParams().algorithm + " but the outlier algorithm has not been initialized. User should call ADsim::setupADalgorithm first");

    m_outlier->linkExecDataMap(&this_step_func_execs);
    anom = m_outlier->run(step);
  }else{
    //Collect outliers and 1 normal event/func into Anomalies object
    for(const auto &exec_it : this_step_execs){
      if(exec_it->get_label() == -1) anom.insert(exec_it, Anomalies::EventType::Outlier);
      else if(anom.nFuncEvents(exec_it->get_fid(), Anomalies::EventType::Normal) == 0) anom.insert(exec_it, Anomalies::EventType::Normal);
    }
  }
  int nanom = anom.nEvents(Anomalies::EventType::Outlier);
  int nnorm = anom.nEvents(Anomalies::EventType::Normal);
  std::cout << "Step " << step << " rank " << m_rid << " Anomalies object contains : " << nanom << " anomalies and " << nnorm << " normal events (max 1)" << std::endl;

  //Extract provenance data and send to the provDB
  if(nanom > 0){
    std::vector<nlohmann::json> normal_events;
    std::vector<nlohmann::json> anomalous_events;

    eventIDmap evmap(m_all_execs, m_eventid_map);
      
    const ParamInterface &params = adAlgorithmParams().algorithm != "none" ? *m_outlier->get_global_parameters() : globalParams();
    ADAnomalyProvenance::getProvenanceEntries(anomalous_events, normal_events, m_normal_events,
					      anom, step, step_start_time, step_end_time, m_window_size,
					      params, evmap, counters, m_metadata);
#ifdef ENABLE_PROVDB
    //Send to provdb
    if(anomalous_events.size() > 0)
      m_pdb_client->sendMultipleData(anomalous_events, ProvenanceDataType::AnomalyData); 
    if(normal_events.size() > 0)
      m_pdb_client->sendMultipleData(normal_events, ProvenanceDataType::NormalExecData);   
#else
    if(anomalous_events.size() > 0)
      m_prov_io->writeJSON(anomalous_events, step, "anomalies");
    if(normal_events.size() > 0)
      m_prov_io->writeJSON(normal_events, step, "normalexecs");   
#endif
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

    ADLocalAnomalyMetrics metrics(m_pid, m_rid, step, step_start_time, step_end_time, anom);
    getPserver().addAnomalyMetrics(metrics);
  }
}


size_t ADsim::nStepExecs(unsigned long step) const{
  auto mse = m_step_execs.find(step);
  if(mse == m_step_execs.end()){
    return 0;
  }else return mse->second.size();
}
  
