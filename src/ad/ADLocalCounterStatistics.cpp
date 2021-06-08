#include <chimbuko/ad/ADLocalCounterStatistics.hpp>

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

using namespace chimbuko;

std::string ADLocalCounterStatistics::State::serialize_cerealpb() const{
  std::stringstream ss;
  {    
    cereal::PortableBinaryOutputArchive wr(ss);
    wr(*this);    
  }
  return ss.str();
}

void ADLocalCounterStatistics::State::deserialize_cerealpb(const std::string &strstate){
  std::stringstream ss; ss << strstate;;
  {    
    cereal::PortableBinaryInputArchive rd(ss);
    rd(*this);    
  }
}


void ADLocalCounterStatistics::gatherStatistics(const CountersByIndex_t &cntrs_by_idx){
  for(auto it : cntrs_by_idx){
    const std::list<CounterDataListIterator_t> &counters = it.second;
    if(counters.size() > 0){
      const std::string &counter_name = (*counters.begin())->get_countername(); //all counters with this index by definition have the same name
      if(m_which_counter == nullptr || m_which_counter->count(counter_name)){
	auto &stats = m_stats[counter_name];
	for(const auto &c : counters)
	  stats.push(c->get_value());
      }
    }
  }
}


nlohmann::json ADLocalCounterStatistics::get_json_state() const{
  nlohmann::json g_info;
  g_info["counters"] = nlohmann::json::array();
  for (auto it : m_stats) { //loop over counter name
    const std::string &name = it.first;

    nlohmann::json obj;
    obj["pid"] = m_program_idx;
    obj["name"] = name;
    obj["stats"] = it.second.get_json_state();
    g_info["counters"].push_back(obj);
  }
  return g_info;
}


ADLocalCounterStatistics::State ADLocalCounterStatistics::get_state() const{
  ADLocalCounterStatistics::State g_info;
  for (auto it : m_stats) { //loop over counter name
    const std::string &name = it.first;

    State::CounterData obj;
    obj.pid = m_program_idx;
    obj.name = name;
    obj.stats = it.second.get_state();
    g_info.counters.push_back(obj);
  }
  return g_info;
}


//std::pair<size_t, size_t> ADLocalCounterStatistics::updateGlobalStatistics(ADNetClient &net_client, int rank, std::string pserver_addr) const{
std::pair<size_t, size_t> ADLocalCounterStatistics::updateGlobalStatistics(ADThreadNetClient &net_client, int rank, std::string pserver_addr) const{
  //nlohmann::json state = get_json_state();
  State state = get_state();
  PerfTimer timer;
  timer.start();
  //auto msgsz = updateGlobalStatistics(net_client, state.dump(), m_step);
  auto msgsz = updateGlobalStatistics(net_client, state.serialize_cerealpb(), m_step, rank, pserver_addr);
  
  if(m_perf != nullptr){
    m_perf->add("counter_stats_stream_update_ms", timer.elapsed_ms());
    m_perf->add("counter_stats_stream_sent_MB", (double)msgsz.first / 1000000.0); // MB
    m_perf->add("counter_stats_stream_recv_MB", (double)msgsz.second / 1000000.0); // MB
  }  
  return msgsz;
}

//std::pair<size_t, size_t> ADLocalCounterStatistics::updateGlobalStatistics(ADNetClient &net_client, const std::string &l_stats, int step, int rank, std::string pserver_addr){
std::pair<size_t, size_t> ADLocalCounterStatistics::updateGlobalStatistics(ADThreadNetClient &net_client, const std::string &l_stats, int step, int rank, std::string pserver_addr){
  if (!net_client.use_ps())
    return std::make_pair(0, 0);

  //ADThreadNetClient thrnet;
  //thrnet.connect_ps(rank, 0, pserver_addr);
  
  Message msg;
  msg.set_info(net_client.get_client_rank(), net_client.get_server_rank(), MessageType::REQ_ADD, MessageKind::COUNTER_STATS, step);
  msg.set_msg(l_stats);
  
  size_t sent_sz = msg.size();
  //std::string strmsg = net_client.send_and_receive(msg);
  net_client.async_send(msg);
  //thrnet.async_send(msg);
  size_t recv_sz = 0; //msg.size(); //strmsg.size();

  //thrnet.disconnect_ps();  
  return std::make_pair(sent_sz, recv_sz);
}

void ADLocalCounterStatistics::setStats(const std::string &counter, const RunStats &to){
  if(m_which_counter != nullptr && !m_which_counter->count(counter)) throw std::runtime_error("Counter is not in the list of counters we are collecting");
  m_stats[counter] = to;
}

