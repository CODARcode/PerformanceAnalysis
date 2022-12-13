#include <chimbuko/ad/ADLocalFuncStatistics.hpp>
#include <chimbuko/ad/AnomalyData.hpp>

#include <chimbuko/util/serialize.hpp>
#include <climits>
#include <algorithm>

using namespace chimbuko;

nlohmann::json ADLocalFuncStatistics::State::get_json() const{
  nlohmann::json g_info;
  g_info["func"] = nlohmann::json::array();
  for(const auto &e : func){
    g_info["func"].push_back(e.get_json());
  }
  g_info["anomaly"] = anomaly.get_json();
  return g_info;
}



std::string ADLocalFuncStatistics::State::serialize_cerealpb() const{
  return cereal_serialize(*this);
}

void ADLocalFuncStatistics::State::deserialize_cerealpb(const std::string &strstate){
  cereal_deserialize(*this, strstate);
}

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

void ADLocalFuncStatistics::gatherAnomalies(const Anomalies &anom){
  //Gather information on the number of anomalies and stats on their scores
  const std::vector<CallListIterator_t> &anomalies = anom.allEventsRecorded(Anomalies::EventType::Outlier);
  m_anom_data.incr_n_anomalies(anomalies.size());
  
  for(auto const &it : anomalies){
    m_anom_data.add_outlier_score(it->get_outlier_score());
    ++m_funcstats[it->get_fid()].n_anomaly; //increment func anomalies count
  }

}

ADLocalFuncStatistics::State ADLocalFuncStatistics::get_state() const{
  // func id --> (name, # anomaly, inclusive run stats, exclusive run stats)
  State g_info;
  for (auto const &fstats : m_funcstats) { //loop over function index
    g_info.func.push_back(fstats.second.get_state());
  }

  g_info.anomaly = m_anom_data.get_state();
  return g_info;
}


void ADLocalFuncStatistics::set_state(const ADLocalFuncStatistics::State &s){
  m_funcstats.clear();
  for(auto const &fstats_s : s.func)
    m_funcstats[fstats_s.id].set_state(fstats_s);
  
  m_anom_data.set_state(s.anomaly);
}




nlohmann::json ADLocalFuncStatistics::get_json_state() const{
  return get_state().get_json();
}


std::string ADLocalFuncStatistics::net_serialize() const{
  return get_state().serialize_cerealpb();
}

void ADLocalFuncStatistics::net_deserialize(const std::string &s){
  State state;
  state.deserialize_cerealpb(s);
  set_state(state);
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
  msg.set_msg(l_stats);
  
  size_t sent_sz = msg.size();
  net_client.async_send(msg);
  size_t recv_sz =0;
  
  return std::make_pair(sent_sz, recv_sz);
}
