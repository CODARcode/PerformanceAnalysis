#include "chimbuko/ad/AnomalyStat.hpp"
#include <sstream>

using namespace chimbuko;

AnomalyData::AnomalyData()
: m_app(0), m_rank(0), m_step(0), m_min_timestamp(0), m_max_timestamp(0),
  m_n_anomalies(0), m_stat_id("")
{
}

AnomalyData::AnomalyData(
    unsigned long app, unsigned long rank, unsigned step,
    unsigned long min_ts, unsigned long max_ts, unsigned long n_anomalies,
    std::string stat_id)
{
    set(app, rank, step, min_ts, max_ts, n_anomalies, stat_id);
}

void AnomalyData::set(
    unsigned long app, unsigned long rank, unsigned step,
    unsigned long min_ts, unsigned long max_ts, unsigned long n_anomalies,
    std::string stat_id
)
{
    m_app = app;
    m_rank = rank;
    m_step = step;
    m_min_timestamp = min_ts;
    m_max_timestamp = max_ts,
    m_n_anomalies = n_anomalies;
    m_stat_id = stat_id;
    if (stat_id.length() == 0)
    {
        m_stat_id = std::to_string(m_app) + ":" + std::to_string(m_rank);
    }
}

AnomalyData::AnomalyData(const std::string& s)
{
    nlohmann::json j = nlohmann::json::parse(s);
    m_app = j["app"];
    m_rank = j["rank"];
    m_step = j["step"];
    m_min_timestamp = j["min_timestamp"];
    m_max_timestamp = j["max_timestamp"];
    m_n_anomalies = j["n_anomalies"];
    m_stat_id = j["stat_id"];
}

nlohmann::json AnomalyData::get_json() const
{
    return {
        {"app", get_app()},
        {"rank", get_rank()},
        {"step", get_step()},
        {"min_timestamp", get_min_ts()},
        {"max_timestamp", get_max_ts()},
        {"n_anomalies", get_n_anomalies()},
        {"stat_id", get_stat_id()}
    };
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


AnomalyStat::AnomalyStat(bool do_accumulate)
{
    m_stats.set_do_accumulate(do_accumulate);
    m_data = new std::list<std::string>();
}

AnomalyStat::~AnomalyStat()
{
    if (m_data) delete m_data;
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

RunStats AnomalyStat::get_stats() {
    std::lock_guard<std::mutex> _(m_mutex);
    return m_stats;
}

std::list<std::string>* AnomalyStat::get_data() {
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
