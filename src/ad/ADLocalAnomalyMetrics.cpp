#include <chimbuko/ad/ADLocalAnomalyMetrics.hpp>
#include <chimbuko/util/serialize.hpp>

using namespace chimbuko;

ADLocalAnomalyMetrics::ADLocalAnomalyMetrics(int app, int rank, int step, unsigned long first_event_ts, unsigned long last_event_ts, const ADExecDataInterface &iface): 
  m_app(app), m_rank(rank), m_step(step), m_first_event_ts(first_event_ts), m_last_event_ts(last_event_ts), m_perf(nullptr){
  
  for(size_t dset_idx =0 ; dset_idx < iface.nDataSets(); dset_idx++){
    size_t fid = iface.getDataSetParamIndex(dset_idx);
    auto const & r = iface.getResults(dset_idx);
    auto const &anom = r.getEventsRecorded(ADExecDataInterface::EventType::Outlier);

    if(anom.size()){
      const std::string & fname = iface.getExecDataEntry(dset_idx,0)->get_funcname();
      auto fit = m_func_anom_metrics.find(fid);
      if(fit == m_func_anom_metrics.end())
	fit = m_func_anom_metrics.emplace(fid, FuncAnomalyMetrics(fid, fname)).first;
      
      for(auto const &e : anom) fit->second.add(*iface.getExecDataEntry(dset_idx,e.index));
    }
  }
}


std::string ADLocalAnomalyMetrics::serialize_cerealpb() const{
  return cereal_serialize(*this);
}

void ADLocalAnomalyMetrics::deserialize_cerealpb(const std::string &strstate){
  cereal_deserialize(*this, strstate);
}


std::string ADLocalAnomalyMetrics::net_serialize() const{
  return serialize_cerealpb();
}

void ADLocalAnomalyMetrics::net_deserialize(const std::string &s){
  deserialize_cerealpb(s);
}



std::pair<size_t, size_t> ADLocalAnomalyMetrics::send(ADNetClient &net_client) const{
  PerfTimer timer;
  timer.start();

  if (!net_client.use_ps())
    return std::make_pair(0, 0);

  if(net_client.get_client_rank() != m_rank) fatal_error("Rank mismatch: net client reported rank " + std::to_string(net_client.get_client_rank()) + " but recorded rank is " + std::to_string(m_rank) );
  
  Message msg;
  msg.set_info(m_rank, net_client.get_server_rank(), MessageType::REQ_ADD, MessageKind::ANOMALY_METRICS, m_step);
  msg.set_msg(net_serialize());
  
  size_t sent_sz = msg.size();
  net_client.async_send(msg);
  size_t recv_sz =0;

  if(m_perf != nullptr){
    m_perf->add("anomaly_metrics_send_update_ms", timer.elapsed_ms());
    m_perf->add("anomaly_metrics_send_sent_MB", (double)sent_sz / 1000000.0); // MB
    m_perf->add("anomaly_metrics_send_recv_MB", (double)recv_sz / 1000000.0); // MB
  }
  
  return std::make_pair(sent_sz, recv_sz);
}

