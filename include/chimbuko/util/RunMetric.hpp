#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
#include "chimbuko/util/RunStats.hpp"

namespace chimbuko {

class RunMetric {
public:
    RunMetric()
    {

    }
    ~RunMetric()
    {

    }

    void add(std::string name, double val)
    {
        if (m_metrics.count(name) == 0)
        {
            m_metrics[name].push(0);
            m_metrics[name].clear();
            m_metrics[name].set_do_accumulate(true);            
        }
        m_metrics[name].push(val);
    }

    void dump(std::string path, std::string filename="metric.json") const
    {
        std::ofstream f;
        f.open(path + "/" + filename);
        
        if (f.is_open() && m_metrics.size()) 
        {
            nlohmann::json j;
            for (auto m: m_metrics)
            {
                j[m.first] = m.second.get_json();
            }
            f << j.dump(2) << std::endl;
        }

        f.close();
    }

private:
    std::unordered_map<std::string, RunStats> m_metrics;
};

};
