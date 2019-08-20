#include "chimbuko/param.hpp"

using namespace chimbuko;

ParamInterface::ParamInterface() : m_anomaly_data(nullptr)
{
    m_anomaly_data = new std::list<std::string>();
}

ParamInterface::~ParamInterface()
{
    if (m_anomaly_data)
        delete m_anomaly_data;
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