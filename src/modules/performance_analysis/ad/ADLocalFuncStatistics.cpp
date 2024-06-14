#include <chimbuko/modules/performance_analysis/ad/ADLocalFuncStatistics.hpp>
#include <chimbuko/modules/performance_analysis/ad/AnomalyData.hpp>
#include <chimbuko/modules/performance_analysis/pserver/PScommon.hpp>
#include <chimbuko/core/util/serialize.hpp>
#include <climits>
#include <algorithm>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

ADLocalFuncStatistics::ADLocalFuncStatistics(const unsigned long program_idx, const unsigned long rank, const int step, PerfStats* perf): 
  m_perf(nullptr), m_anom_data(program_idx, rank, step, 0,0,0){}

void ADLocalFuncStatistics::gatherStatistics(const ExecDataMap_t* exec_data){
  unsigned long min_ts = ULONG_MAX, max_ts = 0;

  for (auto it : *exec_data) { //loop over functions (key is function index)
    if(it.second.size() == 0) continue;

    unsigned long func_id = it.first;
    auto fstats_it = m_funcstats.find(func_id);

    //Create new entry if it doesn't exist
    if(fstats_it == m_funcstats.end()){
      const std::string &name = it.second.front()->get_funcname(); //it.second has already been checked to have size >= 1
      fstats_it = m_funcstats.insert( std::unordered_map<unsigned long, FuncStats>::value_type(func_id, FuncStats(m_anom_data.get_app(), func_id, name)) ).first;
    }

    for (auto itt : it.second) { //loop over events for that function
      fstats_it->second.inclusive.push(static_cast<double>(itt->get_inclusive()));
      fstats_it->second.exclusive.push(static_cast<double>(itt->get_exclusive()));

      min_ts = std::min(min_ts, static_cast<unsigned long>(itt->get_entry()) );
      max_ts = std::max(max_ts, static_cast<unsigned long>(itt->get_exit()));
    }
  }

  m_anom_data.set_min_ts(min_ts);
  m_anom_data.set_max_ts(max_ts);
}

void ADLocalFuncStatistics::gatherAnomalies(const ADExecDataInterface &iface){
  //Gather information on the number of anomalies and stats on their scores
  size_t nanom_tot = 0;
  for(size_t dset_idx =0 ; dset_idx < iface.nDataSets(); dset_idx++){
    size_t fid = iface.getDataSetModelIndex(dset_idx);
    auto const & r = iface.getResults(dset_idx);
    auto const &anom = r.getEventsRecorded(ADDataInterface::EventType::Outlier);
    size_t nanom = anom.size();
    nanom_tot += nanom;
    m_funcstats[fid].n_anomaly += nanom; //increment func anomalies count                                                                                                                                                             
    for(auto const &e : anom) m_anom_data.add_outlier_score(e.score);
  }
  m_anom_data.incr_n_anomalies(nanom_tot);
}




nlohmann::json ADLocalFuncStatistics::get_json() const{
  nlohmann::json g_info;
  g_info["func"] = nlohmann::json::array();
  for(const auto &e : m_funcstats){
    g_info["func"].push_back(e.second.get_json());
  }
  g_info["anomaly"] = m_anom_data.get_json();
  return g_info;
}


std::string ADLocalFuncStatistics::serialize_cerealpb() const{
  return cereal_serialize(*this);
}

void ADLocalFuncStatistics::deserialize_cerealpb(const std::string &strstate){
  cereal_deserialize(*this, strstate);
}


std::string ADLocalFuncStatistics::net_serialize() const{
  return serialize_cerealpb();
}

void ADLocalFuncStatistics::net_deserialize(const std::string &s){
  deserialize_cerealpb(s);
}



std::pair<size_t, size_t> ADLocalFuncStatistics::updateGlobalStatistics(ADThreadNetClient &net_client) const{
  PerfTimer timer;
  timer.start();
  auto msgsz = updateGlobalStatistics(net_client, net_serialize(), m_anom_data.get_step());
  
  if(m_perf != nullptr){
    m_perf->add("func_stats_stream_update_ms", timer.elapsed_ms());
    m_perf->add("func_stats_stream_sent_MB", (double)msgsz.first / 1000000.0); // MB
    m_perf->add("func_stats_stream_recv_MB", (double)msgsz.second / 1000000.0); // MB
  }
  
  return msgsz;
}

std::pair<size_t, size_t> ADLocalFuncStatistics::updateGlobalStatistics(ADThreadNetClient &net_client, const std::string &l_stats, int step){
  if (!net_client.use_ps())
    return std::make_pair(0, 0);

  Message msg;
  msg.set_info(net_client.get_client_rank(), net_client.get_server_rank(), MessageType::REQ_ADD, MessageKind::ANOMALY_STATS, step);
  msg.setContent(l_stats);
  
  size_t sent_sz = msg.size();
  net_client.async_send(msg);
  size_t recv_sz =0;
  
  return std::make_pair(sent_sz, recv_sz);
}
