#include <chimbuko/ad/ADLocalCounterStatistics.hpp>
#include <chimbuko/util/serialize.hpp>

using namespace chimbuko;

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


nlohmann::json ADLocalCounterStatistics::get_json() const{
  nlohmann::json g_info;
  g_info["counters"] = nlohmann::json::array();
  for (auto const &it : m_stats) {
    nlohmann::json obj;
    obj["pid"] = m_program_idx;
    obj["name"] = it.first;
    obj["stats"] = it.second.get_json();
    g_info["counters"].push_back(obj);
  }
  g_info["step"] = m_step;

  return g_info;
}

std::string ADLocalCounterStatistics::serialize_cerealpb() const{
  return cereal_serialize(*this);
}

void ADLocalCounterStatistics::deserialize_cerealpb(const std::string &strstate){
  cereal_deserialize(*this, strstate);
}

std::string ADLocalCounterStatistics::net_serialize() const{
  return serialize_cerealpb();
}

void ADLocalCounterStatistics::net_deserialize(const std::string &s){
  deserialize_cerealpb(s);
}


std::pair<size_t, size_t> ADLocalCounterStatistics::updateGlobalStatistics(ADThreadNetClient &net_client) const{
  PerfTimer timer;
  timer.start();
  auto msgsz = updateGlobalStatistics(net_client, net_serialize(), m_step);
  
  if(m_perf != nullptr){
    m_perf->add("counter_stats_stream_update_ms", timer.elapsed_ms());
    m_perf->add("counter_stats_stream_sent_MB", (double)msgsz.first / 1000000.0); // MB
    m_perf->add("counter_stats_stream_recv_MB", (double)msgsz.second / 1000000.0); // MB
  }  
  return msgsz;
}

std::pair<size_t, size_t> ADLocalCounterStatistics::updateGlobalStatistics(ADThreadNetClient &net_client, const std::string &l_stats, int step){
  if (!net_client.use_ps())
    return std::make_pair(0, 0);

  Message msg;
  msg.set_info(net_client.get_client_rank(), net_client.get_server_rank(), MessageType::REQ_ADD, MessageKind::COUNTER_STATS, step);
  msg.set_msg(l_stats);
  
  size_t sent_sz = msg.size();
  net_client.async_send(msg);
  size_t recv_sz = 0;

  return std::make_pair(sent_sz, recv_sz);
}

void ADLocalCounterStatistics::setStats(const std::string &counter, const RunStats &to){
  if(m_which_counter != nullptr && !m_which_counter->count(counter)) throw std::runtime_error("Counter is not in the list of counters we are collecting");
  m_stats[counter] = to;
}

