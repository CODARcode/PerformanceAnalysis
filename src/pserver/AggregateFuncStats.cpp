#include "chimbuko/pserver/AggregateFuncStats.hpp"
#include "chimbuko/util/error.hpp"

using namespace chimbuko;

AggregateFuncStats::AggregateFuncStats(int pid, int fid, const std::string &func): m_pid(pid), m_fid(fid), m_func(func){
  m_func_anomaly.set_do_accumulate(true); //accumulate number of anomalies as 'accumulate' field
  m_inclusive.set_do_accumulate(true); //also accumulate total runtimes over all rank, thread
  m_exclusive.set_do_accumulate(true);
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

AggregateFuncStats & AggregateFuncStats::operator+=(const AggregateFuncStats &r){
  if(r.m_pid != m_pid || r.m_fid != m_fid || r.m_func != m_func) fatal_error("Instances should refer to the same process/function!");
  m_func_anomaly += r.m_func_anomaly;
  m_inclusive+= r.m_inclusive;
  m_exclusive+= r.m_exclusive;
  return *this;
}
