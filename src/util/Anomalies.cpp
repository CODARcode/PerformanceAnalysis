#include "chimbuko/util/Anomalies.hpp"

using namespace chimbuko;

void Anomalies::insert(CallListIterator_t event, EventType type){
  switch(type){
  case EventType::Outlier:
    m_all_outliers.push_back(event);
    m_func_outliers[event->get_fid()].push_back(event);
    break;
  case EventType::Normal:
    m_all_normal_execs.push_back(event);
    m_func_normal_execs[event->get_fid()].push_back(event);
    break;
  default:
    throw std::runtime_error("Invalid type");
  }
}

void Anomalies::insert(CallListIterator_t event, EventType type, double hbos_score, double threshold) {
  switch(type){
  case EventType::Outlier:
    m_all_outliers.push_back(event);
    m_func_outliers[event->get_fid()].push_back(event);
    if(m_func_outliers_hbos_scores_and_threshold[event->get_fid()].size() == 0) {
      m_func_outliers_hbos_scores_and_threshold[event->get_fid()] = std::vector<std::vector<double> >(2);
    }
    m_func_outliers_hbos_scores_and_threshold[event->get_fid()].at(0).push_back(hbos_score);
    m_func_outliers_hbos_scores_and_threshold[event->get_fid()].at(1).push_back(threshold);
    break;
  case EventType::Normal:
    m_all_normal_execs.push_back(event);
    m_func_normal_execs[event->get_fid()].push_back(event);
    break;
  default:
    throw std::runtime_error("Invalid type");
  }
}

const std::vector<CallListIterator_t> & Anomalies::funcEvents(const unsigned long func_id, EventType type) const{
  static std::vector<CallListIterator_t> empty;
  auto const &mp = type == EventType::Outlier ? m_func_outliers : m_func_normal_execs;
  auto it = mp.find(func_id);
  if(it != mp.end()) return it->second;
  else return empty;
}
