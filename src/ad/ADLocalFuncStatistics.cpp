#include <chimbuko/ad/ADLocalFuncStatistics.hpp>
#include <chimbuko/ad/AnomalyData.hpp>

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

using namespace chimbuko;

ADLocalFuncStatistics::FuncStats::State::State(const ADLocalFuncStatistics::FuncStats &p):  pid(p.pid), id(p.id), name(p.name), n_anomaly(p.n_anomaly), 
										     inclusive(p.inclusive.get_state()), exclusive(p.exclusive.get_state()){}
  

nlohmann::json ADLocalFuncStatistics::FuncStats::State::get_json() const{
  nlohmann::json obj;
  obj["pid"] = pid;
  obj["id"] = id;
  obj["name"] = name;
  obj["n_anomaly"] = n_anomaly;
  obj["inclusive"] = inclusive.get_json();
  obj["exclusive"] = exclusive.get_json();
  return obj;
}

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
  std::stringstream ss;
  {    
    cereal::PortableBinaryOutputArchive wr(ss);
    wr(*this);    
  }
  return ss.str();
}

void ADLocalFuncStatistics::State::deserialize_cerealpb(const std::string &strstate){
  std::stringstream ss; ss << strstate;;
  {    
    cereal::PortableBinaryInputArchive rd(ss);
    rd(*this);    
  }
}

ADLocalFuncStatistics::ADLocalFuncStatistics(const unsigned long program_idx, const unsigned long rank, const int step, PerfStats* perf): 
  m_program_idx(program_idx), m_rank(rank), m_step(step), m_min_ts(0), m_max_ts(0), m_perf(perf), m_n_anomalies(0){}

void ADLocalFuncStatistics::gatherStatistics(const ExecDataMap_t* exec_data){
  for (auto it : *exec_data) { //loop over functions (key is function index)
    if(it.second.size() == 0) continue;

    unsigned long func_id = it.first;
    auto fstats_it = m_funcstats.find(func_id);

    //Create new entry if it doesn't exist
    if(fstats_it == m_funcstats.end()){
      const std::string &name = it.second.front()->get_funcname(); //it.second has already been checked to have size >= 1
      fstats_it = m_funcstats.insert( std::unordered_map<unsigned long, FuncStats>::value_type(func_id, FuncStats(m_program_idx, func_id, name)) ).first;
    }

    for (auto itt : it.second) { //loop over events for that function
      fstats_it->second.inclusive.push(static_cast<double>(itt->get_inclusive()));
      fstats_it->second.exclusive.push(static_cast<double>(itt->get_exclusive()));

      if (m_min_ts == 0 || m_min_ts > itt->get_entry())
	m_min_ts = itt->get_entry();
      if (m_max_ts == 0 || m_max_ts < itt->get_exit())
	m_max_ts = itt->get_exit();      
    }
  }
}

void ADLocalFuncStatistics::gatherAnomalies(const Anomalies &anom){
  //Loop over functions and get the number of anomalies
  for(auto &fstats: m_funcstats){
    unsigned long func_id = fstats.second.id;
    fstats.second.n_anomaly += anom.nFuncEvents(func_id, Anomalies::EventType::Outlier);
  }
  //Total anomalies
  m_n_anomalies += anom.nEvents(Anomalies::EventType::Outlier);
}

ADLocalFuncStatistics::State ADLocalFuncStatistics::get_state() const{
  // func id --> (name, # anomaly, inclusive run stats, exclusive run stats)
  State g_info;
  for (auto const &fstats : m_funcstats) { //loop over function index
    g_info.func.push_back(fstats.second.get_state());
  }

  g_info.anomaly = AnomalyData(m_program_idx, m_rank, m_step, m_min_ts, m_max_ts, m_n_anomalies);
  return g_info;
}
nlohmann::json ADLocalFuncStatistics::get_json_state() const{
  return get_state().get_json();
}

std::pair<size_t, size_t> ADLocalFuncStatistics::updateGlobalStatistics(ADNetClient &net_client) const{
  // func id --> (name, # anomaly, inclusive run stats, exclusive run stats)
  State g_info = get_state();
  PerfTimer timer;
  timer.start();
  auto msgsz = updateGlobalStatistics(net_client, g_info.serialize_cerealpb(), m_step);
  
  if(m_perf != nullptr){
    m_perf->add("func_stats_stream_update_ms", timer.elapsed_ms());
    m_perf->add("func_stats_stream_sent_MB", (double)msgsz.first / 1000000.0); // MB
    m_perf->add("func_stats_stream_recv_MB", (double)msgsz.second / 1000000.0); // MB
  }
  
  return msgsz;
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
