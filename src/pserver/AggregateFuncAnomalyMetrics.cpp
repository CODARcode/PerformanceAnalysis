#include<chimbuko/pserver/AggregateFuncAnomalyMetrics.hpp>
#include<cassert>
#include<limits>

using namespace chimbuko;

AggregateFuncAnomalyMetrics::AggregateFuncAnomalyMetrics(int pid, int rid, int fid, const std::string &func): m_pid(pid), m_rid(rid), m_fid(fid), m_func(func),
													      m_count(true), m_score(true), m_severity(true),
													      m_first_io_step(std::numeric_limits<unsigned long>::max()),m_last_io_step(0),
													      m_first_event_ts(std::numeric_limits<unsigned long>::max()),m_last_event_ts(0){

}

void AggregateFuncAnomalyMetrics::add(const FuncAnomalyMetrics &metrics, unsigned long io_step, unsigned long first_event_ts, unsigned long last_event_ts){
  if(metrics.get_fid() != m_fid) fatal_error("Function index mismatch");

  m_score += metrics.get_score();
  m_severity += metrics.get_severity();

  m_count.push(metrics.get_count());
  
  m_first_io_step = std::min(m_first_io_step, io_step);
  m_last_io_step = std::max(m_last_io_step, io_step);

  m_first_event_ts = std::min(m_first_event_ts, first_event_ts);
  m_last_event_ts = std::max(m_last_event_ts, last_event_ts);
}

nlohmann::json AggregateFuncAnomalyMetrics::get_json() const{
  return {
    {"first_io_step", m_first_io_step},
      {"last_io_step", m_last_io_step},
	{ "min_timestamp", m_first_event_ts },
	  { "max_timestamp", m_last_event_ts },
	    {"count",m_count.get_json()},
	      {"score",m_score.get_json()},
		{"severity",m_severity.get_json()}
  };
}
