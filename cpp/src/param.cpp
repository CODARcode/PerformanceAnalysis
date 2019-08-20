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
    /*
        anomaly data
        - app id
        - rank id
        - step
        - min_timestamp
        - max_timestamp
        - # anomalies
        - stat_id

        create class: used by ADOutlier.hpp and param.hpp
         - this is a part of message, logically...
         - put in message.hpp!!
    */
}