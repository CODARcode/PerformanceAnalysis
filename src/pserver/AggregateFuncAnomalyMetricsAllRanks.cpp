#include<chimbuko/pserver/AggregateFuncAnomalyMetricsAllRanks.hpp>
#include<chimbuko/util/error.hpp>
#include<cassert>
#include<limits>
#include<algorithm>

using namespace chimbuko;

AggregateFuncAnomalyMetricsAllRanks::AggregateFuncAnomalyMetricsAllRanks(int pid, int fid, const std::string &func): m_pid(pid), m_fid(fid), m_func(func),
														     m_count(true), m_score(true), m_severity(true),
														     m_first_io_step(std::numeric_limits<unsigned long>::max()),m_last_io_step(0),
														     m_first_event_ts(std::numeric_limits<unsigned long>::max()),m_last_event_ts(0){

}


void AggregateFuncAnomalyMetricsAllRanks::add(const AggregateFuncAnomalyMetrics &r){
  if(m_pid != r.get_pid() || m_fid != r.get_fid() || m_func != r.get_func()) fatal_error("Data metadata mismatch");
  m_count += r.get_count_stats();
  m_score += r.get_score_stats();
  m_severity += r.get_severity_stats();
  m_first_event_ts = std::min(m_first_event_ts, r.get_first_event_ts());
  m_last_event_ts = std::max(m_last_event_ts, r.get_last_event_ts());
  m_first_io_step = std::min(m_first_io_step, r.get_first_io_step());
  m_last_io_step = std::max(m_last_io_step, r.get_last_io_step());
}

nlohmann::json AggregateFuncAnomalyMetricsAllRanks::get_json() const{
  return {
    {"app", m_pid},
      {"fid", m_fid},
	{"fname", m_func},
	  {"first_io_step", m_first_io_step},
	    {"last_io_step", m_last_io_step},
	      { "min_timestamp", m_first_event_ts },
		{ "max_timestamp", m_last_event_ts },
		  {"count",m_count.get_json()},
		    {"score",m_score.get_json()},
		      {"severity",m_severity.get_json()}
  };
}
