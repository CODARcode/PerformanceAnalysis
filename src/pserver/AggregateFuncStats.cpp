#include "chimbuko/pserver/AggregateFuncStats.hpp"

using namespace chimbuko;

AggregateFuncStats::AggregateFuncStats(int pid, int fid, const std::string &func): m_pid(pid), m_fid(fid), m_func(func){
  m_func_anomaly.set_do_accumulate(true); //accumulate number of anomalies as 'accumulate' field
}

void AggregateFuncStats::add(unsigned long n_anomaly, const RunStats& inclusive, const RunStats& exclusive){
  m_func_anomaly.push(n_anomaly);
  m_inclusive += inclusive;
  m_exclusive += exclusive;
}

nlohmann::json AggregateFuncStats::get_json() const{
  nlohmann::json object;
  object["app"] = m_pid;
  object["fid"] = m_fid;
  object["name"] = m_func;
  object["stats"] = m_func_anomaly.get_json();
  object["inclusive"] = m_inclusive.get_json();
  object["exclusive"] = m_exclusive.get_json();
  return object;
}
