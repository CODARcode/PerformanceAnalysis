#pragma once
#include <mutex>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include "chimbuko/ad/AnomalyStat.hpp"

namespace chimbuko {

class ParamInterface {
public:
    ParamInterface();
    ParamInterface(const std::vector<int>& n_ranks);
    virtual ~ParamInterface();
    virtual void clear() = 0;

    virtual size_t size() const = 0;

    virtual std::string serialize() = 0;
    virtual std::string update(const std::string& parameters, bool flag=false) = 0;
    virtual void assign(const std::string& parameters) = 0;

    virtual void show(std::ostream& os) const = 0;

    // anomaly statistics related ...
    void add_anomaly_data(const std::string& data);
    std::string get_anomaly_stat(const std::string& stat_id);

protected:
    // for parameters of an anomaly detection algorithm
    std::mutex m_mutex; // used to update parameters

    // for anomaly statistics
    std::unordered_map<std::string, AnomalyStat*> m_anomaly_stats;
};

} // end of chimbuko namespace
