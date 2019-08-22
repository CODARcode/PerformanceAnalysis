#include "chimbuko/ad/AnomalyStat.hpp"
#include <sstream>

using namespace chimbuko;

std::string AnomalyData::get_binary()
{
    std::stringstream ss(std::stringstream::in | std::stringstream::out | std::stringstream::binary);
    ss << *this;
    return ss.str();
}

void AnomalyData::set_binary(std::string binary)
{
    std::stringstream ss(std::stringstream::in | std::stringstream::out | std::stringstream::binary);
    ss << binary;
    ss >> *this;
}

nlohmann::json AnomalyData::get_json() const
{
    return {
        {"n_anomaly", get_n_anomalies()},
        {"step", get_step()},
        {"min_timestamp", get_min_ts()},
        {"max_timestamp", get_max_ts()},
        {"stat_id", get_stat_id()}
    };
}

void AnomalyData::show(std::ostream& os) const
{
    os << "App: " << m_app 
        << "\nRank: " << m_rank
        << "\nStep: " << m_step
        << "\ntimestamp: [" << m_min_timestamp
        << ", " << m_max_timestamp 
        << "]\n# Anomalies: " << m_n_anomalies
        << "\nStat id: " << m_stat_id << std::endl;
}

std::ostream& chimbuko::operator<<(std::ostream& os, AnomalyData& d)
{
    os.write((const char*)&d.m_app, sizeof(unsigned long));
    os.write((const char*)&d.m_rank, sizeof(unsigned long));
    os.write((const char*)&d.m_step, sizeof(unsigned long));
    os.write((const char*)&d.m_min_timestamp, sizeof(unsigned long));
    os.write((const char*)&d.m_max_timestamp, sizeof(unsigned long));
    os.write((const char*)&d.m_n_anomalies, sizeof(unsigned long));
    return os;
}

std::istream& chimbuko::operator>>(std::istream& is, AnomalyData& d)
{
    is.read((char*)&d.m_app, sizeof(unsigned long));
    is.read((char*)&d.m_rank, sizeof(unsigned long));
    is.read((char*)&d.m_step, sizeof(unsigned long));
    is.read((char*)&d.m_min_timestamp, sizeof(unsigned long));
    is.read((char*)&d.m_max_timestamp, sizeof(unsigned long));
    is.read((char*)&d.m_n_anomalies, sizeof(unsigned long));
    d.m_stat_id = std::to_string(d.m_app) + ":" + std::to_string(d.m_rank);
    return is;
}

bool chimbuko::operator==(const AnomalyData& a, const AnomalyData& b)
{
    return 
        a.m_app == b.m_app &&
        a.m_rank == b.m_rank &&
        a.m_step == b.m_step &&
        a.m_min_timestamp == b.m_min_timestamp &&
        a.m_max_timestamp == b.m_max_timestamp &&
        a.m_n_anomalies == b.m_n_anomalies;
}

bool chimbuko::operator!=(const AnomalyData& a, const AnomalyData& b)
{
    return 
        a.m_app != b.m_app ||
        a.m_rank != b.m_rank ||
        a.m_step != b.m_step ||
        a.m_min_timestamp != b.m_min_timestamp ||
        a.m_max_timestamp != b.m_max_timestamp ||
        a.m_n_anomalies != b.m_n_anomalies;
}


AnomalyStat::AnomalyStat() : m_data(nullptr)
{
    m_stats.set_do_accumulate(true);
    m_data = new std::list<std::string>();
}

AnomalyStat::~AnomalyStat()
{
    if (m_data) delete m_data;
}

void AnomalyStat::add(AnomalyData& d, bool bStore)
{
    std::lock_guard<std::mutex> _(m_mutex);
    m_stats.push(d.get_n_anomalies());
    // std::cout << "add: " << d.get_stat_id() << ": " << d.get_n_anomalies() << std::endl; 
    if (bStore)
    {
        m_data->push_back(d.get_binary());
    }
    // std::cout << "add: " << d.get_stat_id() << ": " << m_stats << std::endl;    
}

void AnomalyStat::add(const std::string& binary, bool bStore)
{
    std::lock_guard<std::mutex> _(m_mutex);
    AnomalyData data(binary);
    m_stats.push(data.get_n_anomalies());
    if (bStore)
    {
        m_data->push_back(binary);
    }
}

RunStats AnomalyStat::get_stats() {
    std::lock_guard<std::mutex> _(m_mutex);
    return m_stats;
}

std::list<std::string>* AnomalyStat::get_data() {
    std::lock_guard<std::mutex> _(m_mutex);
    return m_data;
}

size_t AnomalyStat::get_n_data() {
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
