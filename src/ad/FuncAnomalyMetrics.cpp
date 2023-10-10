#include <chimbuko/ad/FuncAnomalyMetrics.hpp>

using namespace chimbuko;

void FuncAnomalyMetrics::add(const ExecData_t &event){
  if(event.get_label() != -1) fatal_error("Attempting to push an event that is not an outlier!");
  if(event.get_fid() != m_fid) fatal_error("Function index mismatch");

  m_score.push(event.get_outlier_score());
  m_severity.push(event.get_outlier_severity());
  ++m_count;
}

