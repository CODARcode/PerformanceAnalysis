#include "chimbuko/ad/AnomalyData.hpp"
#include <sstream>
#include <chimbuko/util/serialize.hpp>

using namespace chimbuko;

AnomalyData::AnomalyData()
: m_app(0), m_rank(0), m_step(0), m_min_timestamp(0), m_max_timestamp(0),
  m_n_anomalies(0), m_outlier_scores(true)  //collect sum of scores also
{
}

AnomalyData::AnomalyData(
    unsigned long app, unsigned long rank, unsigned step,
    unsigned long min_ts, unsigned long max_ts, unsigned long n_anomalies): AnomalyData(){
    set(app, rank, step, min_ts, max_ts, n_anomalies);
}

void AnomalyData::set(
    unsigned long app, unsigned long rank, unsigned step,
    unsigned long min_ts, unsigned long max_ts, unsigned long n_anomalies){
    m_app = app;
    m_rank = rank;
    m_step = step;
    m_min_timestamp = min_ts;
    m_max_timestamp = max_ts,
    m_n_anomalies = n_anomalies;
}

nlohmann::json AnomalyData::get_json() const
{
    return {
      {"app", m_app},
      {"rank", m_rank},
      {"step", m_step},
      {"min_timestamp", m_min_timestamp},
      {"max_timestamp", m_max_timestamp},
      {"n_anomalies", m_n_anomalies},
      {"outlier_scores",  m_outlier_scores.get_json() } 
    };
}

bool chimbuko::operator==(const AnomalyData& a, const AnomalyData& b){
    return 
      a.m_app == b.m_app &&
      a.m_rank == b.m_rank &&
      a.m_step == b.m_step &&
      a.m_min_timestamp == b.m_min_timestamp &&
      a.m_max_timestamp == b.m_max_timestamp &&
      a.m_n_anomalies == b.m_n_anomalies &&
      a.m_outlier_scores == b.m_outlier_scores;
}

bool chimbuko::operator!=(const AnomalyData& a, const AnomalyData& b){
  return !(a == b);
}

std::string AnomalyData::serialize_cerealpb() const{
  return cereal_serialize(*this);
}

void AnomalyData::deserialize_cerealpb(const std::string &strstate){
  cereal_deserialize(*this, strstate);
}

std::string AnomalyData::net_serialize() const{
  return serialize_cerealpb();
}

void AnomalyData::net_deserialize(const std::string &s){
  deserialize_cerealpb(s);
}


