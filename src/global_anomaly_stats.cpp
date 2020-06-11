#include <chimbuko/global_anomaly_stats.hpp>

using namespace chimbuko;

GlobalAnomalyStats::GlobalAnomalyStats(const std::vector<int>& n_ranks)
{
    reset_anomaly_stat(n_ranks);
}

void GlobalAnomalyStats::reset_anomaly_stat(const std::vector<int>& n_ranks)
{
    if (n_ranks.size() == 0)
        return;

    if (m_anomaly_stats.size())
    {
        for (auto item: m_anomaly_stats)
        {
            delete item.second;
        }
        m_anomaly_stats.clear();
    }

    // pre-allocate AnomalyStat to avoid race-condition
    for (int app_id = 0; app_id < (int)n_ranks.size(); app_id++) {
        for (int rank_id = 0; rank_id < n_ranks[app_id]; rank_id++){
            std::string stat_id = std::to_string(app_id) + ":" + std::to_string(rank_id);
            m_anomaly_stats[stat_id] = new AnomalyStat(true);
        }
    }
}

GlobalAnomalyStats::~GlobalAnomalyStats()
{
    for (auto pair: m_anomaly_stats)
        delete pair.second;
}

void GlobalAnomalyStats::add_anomaly_data(const std::string& data)
{
    nlohmann::json j = nlohmann::json::parse(data);
    if (j.count("anomaly"))
    {
        AnomalyData d(j["anomaly"].dump());
        if (m_anomaly_stats.count(d.get_stat_id()))
            m_anomaly_stats[d.get_stat_id()]->add(d);
    }

    if (j.count("func"))
    {
        for (auto f: j["func"]) {
            update_func_stat(
                f["id"], f["name"], f["n_anomaly"],
                RunStats::from_strstate(f["inclusive"].dump()), 
                RunStats::from_strstate(f["exclusive"].dump())
            );
        }
    }
}

std::string GlobalAnomalyStats::get_anomaly_stat(const std::string& stat_id) const
{
  if (stat_id.length() == 0 || m_anomaly_stats.count(stat_id) == 0)
    return "";
  
  RunStats stat = m_anomaly_stats.find(stat_id)->second->get_stats();
  return stat.get_json().dump();
}

size_t GlobalAnomalyStats::get_n_anomaly_data(const std::string& stat_id) const
{
    if (stat_id.length() == 0 || m_anomaly_stats.count(stat_id) == 0)
        return 0;

    return m_anomaly_stats.find(stat_id)->second->get_n_data();
}

nlohmann::json GlobalAnomalyStats::collect_stat_data()
{
    nlohmann::json jsonObjects = nlohmann::json::array();
    
    for (auto pair: m_anomaly_stats)
    {
        std::string stat_id = pair.first;
        auto stats = pair.second->get();        
        if (stats.second && stats.second->size())
        {
            nlohmann::json object;
            object["key"] = stat_id;
            object["stats"] = stats.first.get_json();

            object["data"] = nlohmann::json::array();
            for (auto strdata: *stats.second)
            {
                object["data"].push_back(
                    AnomalyData(strdata).get_json()
                );
            }

            jsonObjects.push_back(object);
            delete stats.second;            
        }
    }

    return jsonObjects;
}

nlohmann::json GlobalAnomalyStats::collect_func_data()
{
    nlohmann::json jsonObjects = nlohmann::json::array();
    {
        std::lock_guard<std::mutex> _(m_mutex_func);
        for (auto pair: m_func)
        {
            nlohmann::json object;
            object["fid"] = pair.first;
            object["name"] = pair.second;
            object["stats"] = m_func_anomaly[pair.first].get_json();
            object["inclusive"] = m_inclusive[pair.first].get_json();
            object["exclusive"] = m_exclusive[pair.first].get_json();
            jsonObjects.push_back(object);
        }
    }
    return jsonObjects;
}

nlohmann::json GlobalAnomalyStats::collect()
{
    nlohmann::json object;
    object["anomaly"] = collect_stat_data();
    if (object["anomaly"].size() == 0)
      return nlohmann::json(); //return empty objectOB
    object["func"] = collect_func_data();
    object["created_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch() ).count();
    return object;
}


void GlobalAnomalyStats::update_func_stat(unsigned long id, const std::string& name, 
    unsigned long n_anomaly, const RunStats& inclusive, const RunStats& exclusive)
{
    std::lock_guard<std::mutex> _(m_mutex_func);   
    m_func[id] = name;
    if (m_func_anomaly.count(id) == 0) {
        m_func_anomaly[id].set_do_accumulate(true);
    }
    m_func_anomaly[id].push(n_anomaly);
    m_inclusive[id] += inclusive;
    m_exclusive[id] += exclusive;
}
