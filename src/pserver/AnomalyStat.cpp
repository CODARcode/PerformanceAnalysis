#include "chimbuko/pserver/AnomalyStat.hpp"
#include <sstream>

using namespace chimbuko;

AnomalyStat::AnomalyStat(bool do_accumulate)
{
    m_stats.set_do_accumulate(do_accumulate);
    m_data = new std::list<std::string>();
}

AnomalyStat::~AnomalyStat()
{
    if (m_data) delete m_data;
}




AnomalyStat::AnomalyStat(const AnomalyStat &r): m_stats(r.m_stats), m_data(nullptr){
  if(r.m_data)
    m_data = new std::list<std::string>(*r.m_data); 
}

AnomalyStat::AnomalyStat(AnomalyStat &&r): m_stats(std::move(r.m_stats)), m_data(r.m_data){
  r.m_data = nullptr;
}

AnomalyStat & AnomalyStat::operator=(const AnomalyStat &r){
  m_stats = r.m_stats;
  if(m_data){ delete m_data; m_data = nullptr; }
  if(r.m_data)
    m_data = new std::list<std::string>(*r.m_data); 
  return *this;
}

AnomalyStat & AnomalyStat::operator=(AnomalyStat &&r){
  m_stats = std::move(r.m_stats);
  m_data = r.m_data;
  r.m_data = nullptr;
  return *this;
}


void AnomalyStat::add(const AnomalyData& d, bool bStore)
{
    std::lock_guard<std::mutex> _(m_mutex);
    m_stats.push(d.get_n_anomalies());
    if (bStore)
    {
        m_data->push_back(d.get_json().dump());
    }
}

void AnomalyStat::add(const std::string& str, bool bStore)
{
    std::lock_guard<std::mutex> _(m_mutex);
    AnomalyData data(str);
    m_stats.push(data.get_n_anomalies());
    if (bStore)
    {
        m_data->push_back(str);
    }
}

RunStats AnomalyStat::get_stats() const{
    std::lock_guard<std::mutex> _(m_mutex);
    return m_stats;
}

std::list<std::string>* AnomalyStat::get_data() const{
    std::lock_guard<std::mutex> _(m_mutex);
    return m_data;
}

size_t AnomalyStat::get_n_data() const{
    std::lock_guard<std::mutex> _(m_mutex);
    if (m_data == nullptr) 
        return 0;
    return m_data->size();
}

std::pair<RunStats, std::list<std::string>*> AnomalyStat::get()
{
    std::lock_guard<std::mutex> _(m_mutex);
    std::list<std::string>* data = nullptr;
    if (m_data->size())
    {
        data = m_data;
        m_data = new std::list<std::string>();
    }
    return std::make_pair(m_stats, data);
}
