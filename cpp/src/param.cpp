#include "chimbuko/param.hpp"

using namespace chimbuko;

ParamInterface::ParamInterface()
{
}

ParamInterface::ParamInterface(const std::vector<int>& n_ranks)
{
    if (n_ranks.size() == 0)
        return;

    // pre-allocate AnomalyStat to avoid race-condition
    for (int app_id = 0; app_id < (int)n_ranks.size(); app_id++) {
        for (int rank_id = 0; rank_id < n_ranks[app_id]; rank_id++)
        {
            std::string stat_id = std::to_string(app_id) + ":" + std::to_string(rank_id);
            m_anomaly_stats[stat_id] = new AnomalyStat();
        }
    }
}

ParamInterface::~ParamInterface()
{
    for (auto pair: m_anomaly_stats)
        delete pair.second;
}

void ParamInterface::add_anomaly_data(const std::string& data)
{
    AnomalyData d(data);
    m_anomaly_stats[d.get_stat_id()]->add(d);
}

std::string ParamInterface::get_anomaly_stat(const std::string& stat_id)
{
    if (stat_id.length() == 0 || m_anomaly_stats.count(stat_id) == 0)
        return "";

    RunStats stat = m_anomaly_stats[stat_id]->get_stats();
    return stat.get_binary_state();
}

size_t ParamInterface::get_n_anomaly_data(const std::string& stat_id)
{
    if (stat_id.length() == 0 || m_anomaly_stats.count(stat_id) == 0)
        return 0;

    return m_anomaly_stats[stat_id]->get_n_data();
}

std::string ParamInterface::collect_stat_data()
{
    nlohmann::json jsonObjects = nlohmann::json::array();
    
    for (auto pair: m_anomaly_stats)
    {
        std::string stat_id = pair.first;
        // std::pair<RunStats, std::list<std::string>*> dd = pair.second->get();
        auto stats = pair.second->get();
        if (stats.second && stats.second->size())
        {
            nlohmann::json object;
            object["key"] = stat_id;
            stats.first.to_json(object);

            // nlohmann::json dataObjects = nlohmann::json::array();
            object["data"] = nlohmann::json::array();
            for (auto strdata: *stats.second)
            {
                object["data"].push_back(
                    AnomalyData(strdata).get_json()
                );
                //dataObjects.push_back(AnomalyData(strdata).get_json());
            }

            jsonObjects.push_back(object);

            delete stats.second;            
        }
        // pair.second->get();
    }

    return jsonObjects.dump();
}