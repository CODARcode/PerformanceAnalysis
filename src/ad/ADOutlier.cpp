#include "chimbuko/ad/ADOutlier.hpp"
#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/message.hpp"
#include "chimbuko/verbose.hpp"
#include <mpi.h>
#include <nlohmann/json.hpp>

#ifdef _PERF_METRIC
#include <chrono>
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MilliSec;
typedef std::chrono::microseconds MicroSec;
#endif

using namespace chimbuko;

/* ---------------------------------------------------------------------------
 * Implementation of ADOutlier class
 * --------------------------------------------------------------------------- */
ADOutlier::ADOutlier() 
  : m_execDataMap(nullptr), m_param(nullptr), m_use_ps(false)
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



/* ---------------------------------------------------------------------------
 * Implementation of ADOutlierSSTD class
 * --------------------------------------------------------------------------- */
ADOutlierSSTD::ADOutlierSSTD() : ADOutlier(), m_sigma(6.0) {
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

	std::string strmsg = m_net_client->send_and_receive(msg);

	msg.clear();
        msg.set_msg(strmsg , true);
        size_t recv_sz = msg.size();
        g.assign(msg.buf());
        return std::make_pair(sent_sz, recv_sz);
    }
}

std::pair<size_t, size_t> ADOutlier::update_local_statistics(std::string l_stats, int step)
{
    if (!m_use_ps)
        return std::make_pair(0, 0);

    Message msg;
    msg.set_info(m_net_client->get_client_rank(), m_net_client->get_server_rank(), MessageType::REQ_ADD, MessageKind::ANOMALY_STATS, step);
    msg.set_msg(l_stats);

    size_t sent_sz = msg.size();
    std::string strmsg = m_net_client->send_and_receive(msg);
    size_t recv_sz = strmsg.size();

    return std::make_pair(sent_sz, recv_sz);
}

Anomalies ADOutlierSSTD::run(int step) {
  Anomalies outliers;
  if (m_execDataMap == nullptr) return outliers;

  //If using CUDA without precompiled kernels the first time a function is encountered takes much longer as it does a JIT compile
  //This is worked around by ignoring the first time a function is encountered (per device)
  //Set this environment variable to disable the workaround
  bool cuda_jit_workaround = true;
  if(const char* env_p = std::getenv("CHIMBUKO_DISABLE_CUDA_JIT_WORKAROUND")){
    cuda_jit_workaround = false;
  }     
  
  //Generate the statistics based on this IO step
  SstdParam param;
  std::unordered_map<unsigned long, std::string> l_func; //map of function index to function name
  std::unordered_map<unsigned long, RunStats> l_inclusive; //including child calls
  std::unordered_map<unsigned long, RunStats> l_exclusive; //excluding child calls

  for (auto it : *m_execDataMap) { //loop over functions (key is function index)
    unsigned long func_id = it.first;
    for (auto itt : it.second) { //loop over events for that function
      //Update local counts of number of times encountered
      std::array<unsigned long, 4> fkey({itt->get_pid(), itt->get_rid(), itt->get_tid(), func_id});
      auto encounter_it = m_local_func_exec_count.find(fkey);
      if(encounter_it == m_local_func_exec_count.end())
	encounter_it = m_local_func_exec_count.insert({fkey, 0}).first;
      else
	encounter_it->second++;

      if(!cuda_jit_workaround || encounter_it->second > 0){ //ignore first encounter to avoid including CUDA JIT compiles in stats (later this should be done only for GPU kernels	
	param[func_id].push(static_cast<double>(itt->get_runtime()));
	l_func[func_id] = itt->get_funcname();
	l_inclusive[func_id].push(static_cast<double>(itt->get_inclusive()));
	l_exclusive[func_id].push(static_cast<double>(itt->get_exclusive()));
      }

      
    }
  }

    //Update temp runstats to include information collected previously (synchronizes with the parameter server if connected)
#ifdef _PERF_METRIC
    Clock::time_point t0, t1;
    std::pair<size_t, size_t> msgsz;
    MicroSec usec;

    t0 = Clock::now();
    msgsz = sync_param(&param);
    t1 = Clock::now();
    
    usec = std::chrono::duration_cast<MicroSec>(t1 - t0);

    m_perf.add("param_update", (double)usec.count());
    m_perf.add("param_sent", (double)msgsz.first / 1000000.0); // MB
    m_perf.add("param_recv", (double)msgsz.second / 1000000.0); // MB
#else
    sync_param(&param);
#endif

    // run anomaly detection algorithm
    unsigned long n_outliers = 0;
    long min_ts = 0, max_ts = 0;

    // func id --> (name, # anomaly, inclusive run stats, exclusive run stats)
    nlohmann::json g_info;
    g_info["func"] = nlohmann::json::array();
    for (auto it : *m_execDataMap) { //loop over function index
        const unsigned long func_id = it.first;
        const unsigned long n = compute_outliers(outliers,func_id, it.second, min_ts, max_ts);
        n_outliers += n;

        nlohmann::json obj;
        obj["id"] = func_id;
        obj["name"] = l_func[func_id];
        obj["n_anomaly"] = n;
        obj["inclusive"] = l_inclusive[func_id].get_json_state();
        obj["exclusive"] = l_exclusive[func_id].get_json_state();
        g_info["func"].push_back(obj);
    }
    g_info["anomaly"] = AnomalyData(0, m_rank, step, min_ts, max_ts, n_outliers).get_json();

    // update # anomaly
#ifdef _PERF_METRIC
    t0 = Clock::now();
    msgsz = update_local_statistics(g_info.dump(), step);
    t1 = Clock::now();

    usec = std::chrono::duration_cast<MicroSec>(t1 - t0);

    m_perf.add("stream_update", (double)usec.count());
    m_perf.add("stream_sent", (double)msgsz.first / 1000000.0); // MB
    m_perf.add("stream_recv", (double)msgsz.second / 1000000.0); // MB    
#else
    update_local_statistics(g_info.dump(), step);
#endif

    return outliers;
}

unsigned long ADOutlierSSTD::compute_outliers(Anomalies &outliers,
					      const unsigned long func_id, 
					      std::vector<CallListIterator_t>& data,
					      long& min_ts, long& max_ts){
  
  VERBOSE(std::cout << "Finding outliers in events for func " << func_id << std::endl);
  
  SstdParam& param = *(SstdParam*)m_param;
  if (param[func_id].count() < 2){
    VERBOSE(std::cout << "Less than 2 events in stats associated with that func, stats not complete" << std::endl);
    return 0;
  }
  unsigned long n_outliers = 0;
  max_ts = 0;
  min_ts = 0;

  const double mean = param[func_id].mean();
  const double std = param[func_id].stddev();

  const double thr_hi = mean + m_sigma * std;
  const double thr_lo = mean - m_sigma * std;

  for (auto itt : data) {
    const double runtime = static_cast<double>(itt->get_runtime());
    if (min_ts == 0 || min_ts > itt->get_entry())
      min_ts = itt->get_entry();
    if (max_ts == 0 || max_ts < itt->get_exit())
      max_ts = itt->get_exit();

    int label = (thr_lo > runtime || thr_hi < runtime) ? -1: 1;
    if (label == -1) {
      VERBOSE(std::cout << "!!!!!!!Detected outlier on func id " << func_id << " (" << itt->get_funcname() << ") on thread " << itt->get_tid()
	      << " runtime " << itt->get_runtime() << " mean " << mean << " std " << std << std::endl);
      n_outliers += 1;
      itt->set_label(label);
      outliers.insert(itt); //insert into data structure containing captured anomalies
    }else{
      VERBOSE(std::cout << "Detected normal event on func id " << func_id << " (" << itt->get_funcname() << ") on thread " << itt->get_tid()
	      << " runtime " << itt->get_runtime() << " mean " << mean << " std " << std << std::endl);
    }
  }

  return n_outliers;
}
