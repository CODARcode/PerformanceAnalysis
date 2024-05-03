#include "chimbuko/modules/performance_analysis/pserver/AggregateAnomalyData.hpp"
#include "chimbuko/core/util/string.hpp"
#include "chimbuko/core/util/error.hpp"
#include <sstream>

using namespace chimbuko;

AggregateAnomalyData::AggregateAnomalyData(bool do_accumulate)
{
    m_stats.set_do_accumulate(do_accumulate);
    m_data = new std::list<AnomalyData>();
}

AggregateAnomalyData::~AggregateAnomalyData(){
    if (m_data) delete m_data;
}

AggregateAnomalyData::AggregateAnomalyData(const AggregateAnomalyData &r): m_stats(r.m_stats), m_data(nullptr){
  if(r.m_data)
    m_data = new std::list<AnomalyData>(*r.m_data); 
}

AggregateAnomalyData::AggregateAnomalyData(AggregateAnomalyData &&r): m_stats(std::move(r.m_stats)), m_data(r.m_data){
  r.m_data = nullptr;
}

AggregateAnomalyData & AggregateAnomalyData::operator=(const AggregateAnomalyData &r){
  m_stats = r.m_stats;
  if(m_data){ delete m_data; m_data = nullptr; }
  if(r.m_data)
    m_data = new std::list<AnomalyData>(*r.m_data); 
  return *this;
}

AggregateAnomalyData & AggregateAnomalyData::operator=(AggregateAnomalyData &&r){
  m_stats = std::move(r.m_stats);
  m_data = r.m_data;
  r.m_data = nullptr;
  return *this;
}


void AggregateAnomalyData::add(const AnomalyData& d, bool bStore)
{
  m_stats.push(d.get_n_anomalies());
  if (bStore) m_data->push_back(d);
}

size_t AggregateAnomalyData::get_n_data() const{
  if (m_data == nullptr) 
    return 0;
  return m_data->size();
}

void AggregateAnomalyData::flush(){
  if(m_data->size()){
    delete m_data;
    m_data = new std::list<AnomalyData>();
  }
}

bool AggregateAnomalyData::operator==(const AggregateAnomalyData &r) const{ 
  if(m_stats != r.m_stats) return false;
  if( (m_data != nullptr && r.m_data == nullptr) ||
      (m_data == nullptr && r.m_data != nullptr) )
    return false;
  if(*m_data != *r.m_data) return false;
  return true;
}

nlohmann::json AggregateAnomalyData::get_json(int pid, int rid) const{
  if(!m_data) fatal_error("List pointer should not be null");

  nlohmann::json object;

  //Decide whether to include the data for this pid/rid
  //Do this only if any anomalies were seen since the last call
  bool include = false;
  for(const AnomalyData &adata: *m_data){
    if(adata.get_n_anomalies() > 0){
      include = true;
      break;
    }
  }

  if(include){
    object["key"] = stringize("%d:%d", pid,rid);
    object["stats"] = m_stats.get_json(); //statistics on anomalies to date for this pid/rid
      
    object["data"] = nlohmann::json::array();
    for (const AnomalyData &adata: *m_data){
      //Don't include data for which there are no anomalies
      if(adata.get_n_anomalies()>0)
	object["data"].push_back(adata.get_json());
    }
  }

  return object;
}

AggregateAnomalyData & AggregateAnomalyData::operator+=(const AggregateAnomalyData &r){
  m_stats += r.m_stats;
  if(m_data == nullptr || r.m_data == nullptr) fatal_error("List pointers should not be null!");
  m_data->insert(m_data->end(), r.m_data->begin(), r.m_data->end());
  return *this;
}
