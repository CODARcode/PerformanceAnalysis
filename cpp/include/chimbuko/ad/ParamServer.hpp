#pragma once
#include "RunStats.hpp"
#include <unordered_map>
#include <mutex>

namespace AD {

class ParamServer {

public:
    ParamServer();
    ~ParamServer();

    void clear();
    void update(std::unordered_map<unsigned long, RunStats>& runstats);

private:
    std::mutex m_mutex;
    std::unordered_map<unsigned long, RunStats> m_runstats;
};

}