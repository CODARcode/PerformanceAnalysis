#include<chimbuko/pserver/NetPayloadRecvCombinedADdata.hpp>
#include<chimbuko/ad/ADcombinedPSdata.hpp>

using namespace chimbuko;

void NetPayloadRecvCombinedADdata::action(Message &response, const Message &message){
  check(message);
  if(m_global_anom_stats == nullptr) throw std::runtime_error("Cannot update global anomaly statistics as stats object has not been linked");
  if(m_global_counter_stats == nullptr) throw std::runtime_error("Cannot update global counter statistics as stats object has not been linked");
  if(m_global_anom_metrics == nullptr) throw std::runtime_error("Cannot update global anomaly metrics as stats object has not been linked");
  
  ADLocalFuncStatistics locf;
  ADLocalCounterStatistics locc(0,0,nullptr);
  ADLocalAnomalyMetrics locm;

  ADcombinedPSdata comb(locf,locc,locm);
  comb.net_deserialize(message.buf());

  m_global_anom_stats->add_anomaly_data(locf);
  m_global_counter_stats->add_counter_data(locc);
  m_global_anom_metrics->add(locm);

  response.set_msg("", false);
}
  
