#include <chimbuko/ad/FuncAnomalyMetrics.hpp>

using namespace chimbuko;

FuncAnomalyMetrics::State FuncAnomalyMetrics::get_state() const{
  return State(*this);
}

void FuncAnomalyMetrics::set_state(const FuncAnomalyMetrics::State &s){
  m_score.set_state(s.score);
  m_severity.set_state(s.severity);
  m_count = s.count;
  m_fid = s.fid;
  m_func = s.func;
}

void FuncAnomalyMetrics::add(const ExecData_t &event){
  if(event.get_label() != -1) fatal_error("Attempting to push an event that is not an outlier!");
  if(event.get_fid() != m_fid) fatal_error("Function index mismatch");

  m_score.push(event.get_outlier_score());
  m_severity.push(event.get_outlier_severity());
  ++m_count;
}

