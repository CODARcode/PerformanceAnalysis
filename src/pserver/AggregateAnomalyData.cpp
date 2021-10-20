#include "chimbuko/pserver/AggregateAnomalyData.hpp"
#include "chimbuko/util/string.hpp"
#include <sstream>

using namespace chimbuko;

AggregateAnomalyData::AggregateAnomalyData(bool do_accumulate)
{
    m_stats.set_do_accumulate(do_accumulate);
    m_data = new std::list<AnomalyData>();
}

AggregateAnomalyData::~AggregateAnomalyData()
{
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
    std::lock_guard<std::mutex> _(m_mutex);
    m_stats.push(d.get_n_anomalies());
    if (bStore)
    {
        m_data->push_back(d);
    }
}

RunStats AggregateAnomalyData::get_stats() const{
    std::lock_guard<std::mutex> _(m_mutex);
    return m_stats;
}

std::list<AnomalyData>* AggregateAnomalyData::get_data() const{
    std::lock_guard<std::mutex> _(m_mutex);
    return m_data;
}

size_t AggregateAnomalyData::get_n_data() const{
    std::lock_guard<std::mutex> _(m_mutex);
    if (m_data == nullptr) 
        return 0;
    return m_data->size();
}

std::pair<RunStats, std::list<AnomalyData>*> AggregateAnomalyData::get()
{
    std::lock_guard<std::mutex> _(m_mutex);
    std::list<AnomalyData>* data = nullptr;
    if (m_data->size())
    {
        data = m_data;
        m_data = new std::list<AnomalyData>();
    }
    return std::make_pair(m_stats, data);
}

bool AggregateAnomalyData::operator==(const AggregateAnomalyData &r) const{ 
  if(m_stats != r.m_stats) return false;
  if( (m_data != nullptr && r.m_data == nullptr) ||
      (m_data == nullptr && r.m_data != nullptr) )
    return false;
  if(*m_data != *r.m_data) return false;
  return true;
}

nlohmann::json AggregateAnomalyData::get_json_and_flush(int pid, int rid){
  auto stats = this->get(); //returns a std::pair<RunStats, std::list<AnomalyData>*>,  and flushes the state of pair.second. 
  //We now own the std::list<AnomalyData>* pointer and have to delete it
      
  nlohmann::json object;

  if(stats.second){
    //Decide whether to include the data for this pid/rid
    //Do this only if any anomalies were seen since the last call
    bool include = false;
    for(const AnomalyData &adata: *stats.second){
      if(adata.get_n_anomalies() > 0){
	include = true;
	break;
      }
    }

    if(include){
      object["key"] = stringize("%d:%d", pid,rid);
      object["stats"] = stats.first.get_json(); //statistics on anomalies to date for this pid/rid
      
      object["data"] = nlohmann::json::array();
      for (const AnomalyData &adata: *stats.second){
	//Don't include data for which there are no anomalies
	if(adata.get_n_anomalies()>0)
	  object["data"].push_back(adata.get_json());
      }
    }
    delete stats.second;
  }
  return object;
}
