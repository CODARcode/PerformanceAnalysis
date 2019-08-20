#pragma once
#include <mutex>
#include <string>
#include <iostream>
#include <list>
#include <unordered_map>
#include "chimbuko/util/RunStats.hpp"

namespace chimbuko {

class ParamInterface {
public:
    typedef std::unordered_map<std::string, RunStats> RunStatsMap_t; 

public:
    ParamInterface();
    virtual ~ParamInterface();
    virtual void clear() = 0;

    virtual size_t size() const = 0;

    virtual std::string serialize() = 0;
    virtual std::string update(const std::string& parameters, bool flag=false) = 0;
    virtual void assign(const std::string& parameters) = 0;

    virtual void show(std::ostream& os) const = 0;

    // anomaly statistics related ...
    void add_anomaly_data(const std::string& data);

protected:
    // for parameters of an anomaly detection algorithm
    std::mutex m_mutex; // used to update parameters

    // for anomaly statistics
    std::mutex m_data_lock; // used to update list
    std::list<std::string>  * m_anomaly_data;
    RunStatsMap_t             m_anomaly_stats;
};

} // end of chimbuko namespace
