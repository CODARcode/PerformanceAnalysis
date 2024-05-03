#include<chimbuko/modules/performance_analysis/pserver/NetPayloadRecvCombinedADdata.hpp>
#include<chimbuko/modules/performance_analysis/ad/ADcombinedPSdata.hpp>

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

void NetPayloadRecvCombinedADdataArray::action(Message &response, const Message &message){
  check(message);
  if(m_global_anom_stats == nullptr) throw std::runtime_error("Cannot update global anomaly statistics as stats object has not been linked");
  if(m_global_counter_stats == nullptr) throw std::runtime_error("Cannot update global counter statistics as stats object has not been linked");
  if(m_global_anom_metrics == nullptr) throw std::runtime_error("Cannot update global anomaly metrics as stats object has not been linked");
  
  std::vector<ADLocalFuncStatistics> locf;
  std::vector<ADLocalCounterStatistics> locc;
  std::vector<ADLocalAnomalyMetrics> locm;

  ADcombinedPSdataArray comb(locf,locc,locm);
  comb.net_deserialize(message.buf());

  int N = locf.size();
  if(locc.size() != N || locm.size() != N) fatal_error("Expect consistent size for arrays");

  for(int i=0;i<N;i++){
    m_global_anom_stats->add_anomaly_data(locf[i]);
    m_global_counter_stats->add_counter_data(locc[i]);
    m_global_anom_metrics->add(locm[i]);
  }

  response.set_msg("", false);
}
  
