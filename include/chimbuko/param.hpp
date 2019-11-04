#pragma once
#include <mutex>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
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
    void reset_anomaly_stat(const std::vector<int>& n_ranks);
    void add_anomaly_data(const std::string& data);
    std::string get_anomaly_stat(const std::string& stat_id);
    size_t get_n_anomaly_data(const std::string& stat_id);
    void update_func_stat(unsigned long id, 
        const std::string& name, 
        unsigned long n_anomaly,
        const RunStats& inclusive, 
        const RunStats& exclusive);
    nlohmann::json collect_stat_data();
    nlohmann::json collect_func_data();
    std::string collect();

protected:
    // for parameters of an anomaly detection algorithm
    std::mutex m_mutex; // used to update parameters

    // for global anomaly statistics
    std::unordered_map<std::string, AnomalyStat*> m_anomaly_stats;
    // for global function statistics
    std::mutex m_mutex_func;
    std::unordered_map<unsigned long, std::string> m_func;
    std::unordered_map<unsigned long, RunStats> m_func_anomaly;
    std::unordered_map<unsigned long, RunStats> m_inclusive;
    std::unordered_map<unsigned long, RunStats> m_exclusive;
};

} // end of chimbuko namespace
