#include "chimbuko/param.hpp"
#include <sstream>

using namespace chimbuko;

ParamInterface::ParamInterface()
{
}

ParamInterface::ParamInterface(const std::vector<int>& n_ranks)
{
    reset_anomaly_stat(n_ranks);
}

void ParamInterface::reset_anomaly_stat(const std::vector<int>& n_ranks)
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

ParamInterface::~ParamInterface()
{
    for (auto pair: m_anomaly_stats)
        delete pair.second;
}

void ParamInterface::add_anomaly_data(const std::string& data)
{
    std::stringstream ss(data);
    std::string line;

    std::getline(ss, line, ' ');
    AnomalyData d(line);
    while (std::getline(ss, line, '@'))
    {
        unsigned long func_id = (unsigned long)std::atol(line.c_str());
        unsigned long n_anomaly;
        std::string fname, inclusive, exclusive;
        std::getline(ss, fname, '@');
        std::getline(ss, line, '@');
        n_anomaly = (unsigned long)std::atol(line.c_str());
        std::getline(ss, inclusive, ' ');
        std::getline(ss, exclusive, ' ');
        update_func_stat(func_id, fname, n_anomaly, inclusive, exclusive);
    }

    if (m_anomaly_stats.count(d.get_stat_id()))
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

nlohmann::json ParamInterface::collect_stat_data()
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
            stats.first.to_json(object);

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

nlohmann::json ParamInterface::collect_func_data()
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

std::string ParamInterface::collect()
{
    nlohmann::json object;
    object["stat"] = collect_stat_data();
    if (object["stat"].size() == 0)
        return "";
    object["func"] = collect_func_data();
    return object.dump();
}


void ParamInterface::update_func_stat(unsigned long id, const std::string& name, 
    unsigned long n_anomaly, const std::string& inclusive, const std::string& exclusive)
{
    RunStats _inclusive = RunStats::from_binary_state(inclusive);
    RunStats _exclusive = RunStats::from_binary_state(exclusive);
    {
        std::lock_guard<std::mutex> _(m_mutex_func);   
        m_func[id] = name;
        if (m_func_anomaly.count(id) == 0) {
            m_func_anomaly[id] = RunStats();
            m_func_anomaly[id].set_do_accumulate(true);
        }
        m_func_anomaly[id].push(n_anomaly);
        m_inclusive[id] += _inclusive;
        m_exclusive[id] += _exclusive;
    }
}
