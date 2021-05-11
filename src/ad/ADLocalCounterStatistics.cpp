#include <chimbuko/ad/ADLocalCounterStatistics.hpp>
#include <chimbuko/util/serialize.hpp>

using namespace chimbuko;

std::string ADLocalCounterStatistics::State::serialize_cerealpb() const{
  return cereal_serialize(*this);
}

void ADLocalCounterStatistics::State::deserialize_cerealpb(const std::string &strstate){
  cereal_deserialize(*this, strstate);
}

nlohmann::json ADLocalCounterStatistics::State::get_json() const{
  nlohmann::json g_info;
  g_info["counters"] = nlohmann::json::array();
  for (auto const &it : counters) { //loop over counter name
    nlohmann::json obj;
    obj["pid"] = it.pid;
    obj["name"] = it.name;
    obj["stats"] = it.stats.get_json();
    g_info["counters"].push_back(obj);
  }
  g_info["step"] = step;

  return g_info;
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
  return get_state().get_json();
}


ADLocalCounterStatistics::State ADLocalCounterStatistics::get_state() const{
  ADLocalCounterStatistics::State g_info;
  for (auto const &it : m_stats) { //loop over counter name
    const std::string &name = it.first;

    State::CounterData obj;
    obj.pid = m_program_idx;
    obj.name = name;
    obj.stats = it.second.get_state();
    g_info.counters.push_back(obj);
  }
  g_info.step = m_step;

  return g_info;
}

void ADLocalCounterStatistics::set_state(const ADLocalCounterStatistics::State &s){
  m_step = s.step;
  
  for(auto const &it : s.counters){
    m_program_idx = it.pid;
    m_stats[it.name].set_state(it.stats);
  }
}

std::string ADLocalCounterStatistics::net_serialize() const{
  return get_state().serialize_cerealpb();
}

void ADLocalCounterStatistics::net_deserialize(const std::string &s){
  State state;
  state.deserialize_cerealpb(s);
  set_state(state);
}


std::pair<size_t, size_t> ADLocalCounterStatistics::updateGlobalStatistics(ADNetClient &net_client) const{
  //nlohmann::json state = get_json_state();
  State state = get_state();
  PerfTimer timer;
  timer.start();
  //auto msgsz = updateGlobalStatistics(net_client, state.dump(), m_step);
  auto msgsz = updateGlobalStatistics(net_client, state.serialize_cerealpb(), m_step);
  
  if(m_perf != nullptr){
    m_perf->add("counter_stats_stream_update_ms", timer.elapsed_ms());
    m_perf->add("counter_stats_stream_sent_MB", (double)msgsz.first / 1000000.0); // MB
    m_perf->add("counter_stats_stream_recv_MB", (double)msgsz.second / 1000000.0); // MB
  }  
  return msgsz;
}

std::pair<size_t, size_t> ADLocalCounterStatistics::updateGlobalStatistics(ADNetClient &net_client, const std::string &l_stats, int step){
  if (!net_client.use_ps())
    return std::make_pair(0, 0);
  
  Message msg;
  msg.set_info(net_client.get_client_rank(), net_client.get_server_rank(), MessageType::REQ_ADD, MessageKind::COUNTER_STATS, step);
  msg.set_msg(l_stats);
  
  size_t sent_sz = msg.size();
  std::string strmsg = net_client.send_and_receive(msg);
  size_t recv_sz = strmsg.size();
  
  return std::make_pair(sent_sz, recv_sz);
}

void ADLocalCounterStatistics::setStats(const std::string &counter, const RunStats &to){
  if(m_which_counter != nullptr && !m_which_counter->count(counter)) throw std::runtime_error("Counter is not in the list of counters we are collecting");
  m_stats[counter] = to;
}

