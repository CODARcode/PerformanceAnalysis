#include <chimbuko/ad/ADLocalFuncStatistics.hpp>

#ifdef _PERF_METRIC
#include <chrono>
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MilliSec;
typedef std::chrono::microseconds MicroSec;
#endif

using namespace chimbuko;

void ADLocalFuncStatistics::gatherStatistics(const ExecDataMap_t* exec_data){
  for (auto it : *exec_data) { //loop over functions (key is function index)
    unsigned long func_id = it.first;
    for (auto itt : it.second) { //loop over events for that function
      m_func[func_id] = itt->get_funcname();
      m_inclusive[func_id].push(static_cast<double>(itt->get_inclusive()));
      m_exclusive[func_id].push(static_cast<double>(itt->get_exclusive()));

      if (m_min_ts == 0 || m_min_ts > itt->get_entry())
	m_min_ts = itt->get_entry();
      if (m_max_ts == 0 || m_max_ts < itt->get_exit())
	m_max_ts = itt->get_exit();      
    }
  }
}

void ADLocalFuncStatistics::gatherAnomalies(const Anomalies &anom){
  for(auto fit: m_func){
    unsigned long func_id = fit.first;
    m_anomaly_count[func_id] += anom.nFuncOutliers(func_id);
  }
  m_n_anomalies += anom.nOutliers();
}

std::pair<size_t, size_t> ADLocalFuncStatistics::updateGlobalStatistics(ADNetClient &net_client) const{
  // func id --> (name, # anomaly, inclusive run stats, exclusive run stats)
  nlohmann::json g_info;
  g_info["func"] = nlohmann::json::array();
  for (auto it : m_func) { //loop over function index
    const unsigned long func_id = it.first;
    const unsigned long n = m_anomaly_count.find(func_id)->second;

    nlohmann::json obj;
    obj["id"] = func_id;
    obj["name"] = it.second;
    obj["n_anomaly"] = n;
    obj["inclusive"] = m_inclusive.find(func_id)->second.get_json_state();
    obj["exclusive"] = m_exclusive.find(func_id)->second.get_json_state();
    g_info["func"].push_back(obj);
  }
  g_info["anomaly"] = AnomalyData(0, net_client.get_client_rank(), m_step, m_min_ts, m_max_ts, m_n_anomalies).get_json();
#ifndef _PERF_METRIC
  return updateGlobalStatistics(net_client, g_info.dump(), m_step);
#else

  Clock::time_point t0 = Clock::now();
  auto msgsz = updateGlobalStatistics(net_client, g_info.dump(), m_step);
  Clock::time_point t1 = Clock::now();

  MicroSec usec = std::chrono::duration_cast<MicroSec>(t1 - t0);
  
  if(m_perf != nullptr){
    m_perf->add("stream_update", (double)usec.count());
    m_perf->add("stream_sent", (double)msgsz.first / 1000000.0); // MB
    m_perf->add("stream_recv", (double)msgsz.second / 1000000.0); // MB
  }
  
  return msgsz;
#endif
}

std::pair<size_t, size_t> ADLocalFuncStatistics::updateGlobalStatistics(ADNetClient &net_client, const std::string &l_stats, int step){
  if (!net_client.use_ps())
    return std::make_pair(0, 0);
  
  Message msg;
  msg.set_info(net_client.get_client_rank(), net_client.get_server_rank(), MessageType::REQ_ADD, MessageKind::ANOMALY_STATS, step);
  msg.set_msg(l_stats);
  
  size_t sent_sz = msg.size();
  std::string strmsg = net_client.send_and_receive(msg);
  size_t recv_sz = strmsg.size();
  
  return std::make_pair(sent_sz, recv_sz);
}
