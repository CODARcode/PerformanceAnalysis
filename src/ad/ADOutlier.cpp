#include "chimbuko/ad/ADOutlier.hpp"
#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/message.hpp"
#include "chimbuko/verbose.hpp"
#include <mpi.h>
#include <nlohmann/json.hpp>

using namespace chimbuko;

/* ---------------------------------------------------------------------------
 * Implementation of ADOutlier class
 * --------------------------------------------------------------------------- */
ADOutlier::ADOutlier(OutlierStatistic stat) 
  : m_execDataMap(nullptr), m_param(nullptr), m_use_ps(false), m_perf(nullptr), m_statistic(stat)
{
}

ADOutlier::~ADOutlier() {
    if (m_param) {
        delete m_param;
    }
}

void ADOutlier::linkNetworkClient(ADNetClient *client){
  m_net_client = client;
  m_use_ps = (m_net_client != nullptr && m_net_client->use_ps());
}

double ADOutlier::getStatisticValue(const ExecData_t &e) const{
  switch(m_statistic){
  case ExclusiveRuntime:
    return e.get_exclusive();
  case InclusiveRuntime:
    return e.get_inclusive();
  default:
    throw std::runtime_error("Invalid statistic");
  }
}
 
/* ---------------------------------------------------------------------------
 * Implementation of ADOutlierSSTD class
 * --------------------------------------------------------------------------- */
ADOutlierSSTD::ADOutlierSSTD(OutlierStatistic stat) : ADOutlier(stat), m_sigma(6.0) {
    m_param = new SstdParam();
}

ADOutlierSSTD::~ADOutlierSSTD() {
}

std::pair<size_t,size_t> ADOutlierSSTD::sync_param(ParamInterface const* param)
{
  SstdParam& g = *(SstdParam*)m_param; //global parameter set
  const SstdParam & l = *(SstdParam const*)param; //local parameter set

    if (!m_use_ps) {
        g.update(l);
        return std::make_pair(0, 0);
    }
    else {
        Message msg;
        msg.set_info(m_net_client->get_client_rank(), m_net_client->get_server_rank(), MessageType::REQ_ADD, MessageKind::PARAMETERS);
        msg.set_msg(l.serialize(), false);
        size_t sent_sz = msg.size();

	m_net_client->send_and_receive(msg, msg);
        size_t recv_sz = msg.size();
        g.assign(msg.buf());
        return std::make_pair(sent_sz, recv_sz);
    }
}

Anomalies ADOutlierSSTD::run(int step) {
  Anomalies outliers;
  if (m_execDataMap == nullptr) return outliers;

  //If using CUDA without precompiled kernels the first time a function is encountered takes much longer as it does a JIT compile
  //Python scripts also appear to take longer executing a function the first time
  //This is worked around by ignoring the first time a function is encountered (per device)
  //Set this environment variable to disable the workaround
  bool cuda_jit_workaround = true;
  if(const char* env_p = std::getenv("CHIMBUKO_DISABLE_CUDA_JIT_WORKAROUND")){
    cuda_jit_workaround = false;
  }     
  
  //Generate the statistics based on this IO step
  SstdParam param;
  for (auto it : *m_execDataMap) { //loop over functions (key is function index)
    unsigned long func_id = it.first;
    for (auto itt : it.second) { //loop over events for that function
      if(itt->get_label() == 0){ //has not been analyzed previously
	//Update local counts of number of times encountered
	std::array<unsigned long, 4> fkey({itt->get_pid(), itt->get_rid(), itt->get_tid(), func_id});
	auto encounter_it = m_local_func_exec_count.find(fkey);
	if(encounter_it == m_local_func_exec_count.end())
	  encounter_it = m_local_func_exec_count.insert({fkey, 0}).first;
	else
	  encounter_it->second++;
	
	if(!cuda_jit_workaround || encounter_it->second > 0){ //ignore first encounter to avoid including CUDA JIT compiles in stats (later this should be done only for GPU kernels	
	  param[func_id].push( this->getStatisticValue(*itt) );
	}
      }
    }
  }

  //Update temp runstats to include information collected previously (synchronizes with the parameter server if connected)
  PerfTimer timer;
  timer.start();
  std::pair<size_t, size_t> msgsz = sync_param(&param);

  if(m_perf != nullptr){
    m_perf->add("param_update_us", timer.elapsed_us());
    m_perf->add("param_sent_MB", (double)msgsz.first / 1000000.0); // MB
    m_perf->add("param_recv_MB", (double)msgsz.second / 1000000.0); // MB
  }

  //Run anomaly detection algorithm
  for (auto it : *m_execDataMap) { //loop over function index
    const unsigned long func_id = it.first;
    const unsigned long n = compute_outliers(outliers,func_id, it.second);
  }
  return outliers;
}

unsigned long ADOutlierSSTD::compute_outliers(Anomalies &outliers,
					      const unsigned long func_id, 
					      std::vector<CallListIterator_t>& data){
  
  verboseStream << "Finding outliers in events for func " << func_id << std::endl;
  
  SstdParam& param = *(SstdParam*)m_param;
  if (param[func_id].count() < 2){
    verboseStream << "Less than 2 events in stats associated with that func, stats not complete" << std::endl;
    return 0;
  }
  unsigned long n_outliers = 0;

  const double mean = param[func_id].mean();
  const double std = param[func_id].stddev();

  const double thr_hi = mean + m_sigma * std;
  const double thr_lo = mean - m_sigma * std;

  for (auto itt : data) {
    if(itt->get_label() == 0){ //only label new events
      const double runtime = this->getStatisticValue(*itt);
      int label = (thr_lo > runtime || thr_hi < runtime) ? -1: 1;
      itt->set_label(label);
      if (label == -1) {
	verboseStream << "!!!!!!!Detected outlier on func id " << func_id << " (" << itt->get_funcname() << ") on thread " << itt->get_tid()
		      << " runtime " << runtime << " mean " << mean << " std " << std << std::endl;
	n_outliers += 1;
	outliers.insert(itt, Anomalies::EventType::Outlier); //insert into data structure containing captured anomalies
      }else{
	//Capture maximum of one normal execution per io step
	if(outliers.nFuncEvents(func_id, Anomalies::EventType::Normal) == 0){
	  verboseStream << "Detected normal event on func id " << func_id << " (" << itt->get_funcname() << ") on thread " << itt->get_tid()
			<< " runtime " << runtime << " mean " << mean << " std " << std << std::endl;
	  
	  outliers.insert(itt, Anomalies::EventType::Normal);      
	}
      }
    }
  }

  return n_outliers;
}
