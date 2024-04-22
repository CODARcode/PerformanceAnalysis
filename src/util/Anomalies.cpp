#include "chimbuko/util/Anomalies.hpp"
#include "chimbuko/util/error.hpp"
#include "chimbuko/verbose.hpp"

using namespace chimbuko;

void Anomalies::recordAnomaly(CallListIterator_t event){
  if(event->get_label() != -1){ fatal_error("Event is not an anomaly!"); }

  m_all_outliers.push_back(event);
  ++m_n_events_total; //increment counter of all events
}

bool Anomalies::recordNormalEventConditional(CallListIterator_t event){
  if(event->get_label() != 1){ fatal_error("Event is not a normal event!"); }
  ++m_n_events_total; //increment counter of all events

  auto eit = m_func_normal_exec_idx.find(event->get_fid());
  if(eit == m_func_normal_exec_idx.end()){ //Add the event to the array and mark its location
    m_all_normal_execs.push_back(event);
    m_func_normal_exec_idx[event->get_fid()] = m_all_normal_execs.size()-1;
    return true;
  }else if(event->get_outlier_score() < m_all_normal_execs[eit->second]->get_outlier_score()){  //Replace the existing normal execution if the current has a lower score
    m_all_normal_execs[eit->second] = event;
    return true;
  }
  return false;
}

size_t Anomalies::nFuncEventsRecorded(unsigned long fid, EventType type) const{
  size_t out = 0;
  if(type == EventType::Outlier){
    for(auto it : m_all_outliers)
      if(it->get_fid() == fid) ++out;
  }else if(m_func_normal_exec_idx.count(fid)){
    out = 1;
  }
  return out;
}

std::vector<CallListIterator_t> Anomalies::funcEventsRecorded(unsigned long fid, EventType type) const{
  std::vector<CallListIterator_t> out;
  if(type == EventType::Outlier){
    for(auto it : m_all_outliers)
      if(it->get_fid() == fid) out.push_back(it);
  }else if(m_func_normal_exec_idx.count(fid)){
    out.push_back( m_all_normal_execs[m_func_normal_exec_idx.find(fid)->second] );
  }
  return out;
}

void Anomalies::import(const ADExecDataInterface &iface){
  m_n_events_total = iface.nEvents();
  m_all_outliers.resize(iface.nEventsRecorded(ADExecDataInterface::EventType::Outlier));
  m_all_normal_execs.resize(iface.nEventsRecorded(ADExecDataInterface::EventType::Normal));
  verboseStream << "Anomalies::import  importing " << m_all_outliers.size() << " outliers and " << m_all_normal_execs.size() << " normal events" << std::endl;
  size_t i=0,j=0;
  for(size_t dset=0;dset<iface.nDataSets();dset++){
    for(auto const &e : iface.getResults(dset).getEventsRecorded(ADExecDataInterface::EventType::Outlier))
      m_all_outliers[i++] = iface.getExecDataEntry(dset,e.index);
    for(auto const &e : iface.getResults(dset).getEventsRecorded(ADExecDataInterface::EventType::Normal)){
      m_all_normal_execs[j] = iface.getExecDataEntry(dset,e.index);
      size_t fid = m_all_normal_execs[j]->get_fid();
      m_func_normal_exec_idx[fid] = j++;
    }
  }
}
