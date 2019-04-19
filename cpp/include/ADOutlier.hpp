#pragma once
#include "ADEvent.hpp"
#include "ExecData.hpp"
#include "RunStats.hpp"

class ADOutlier {

public:
    ADOutlier();
    ~ADOutlier();

    void linkExecDataMap(const ExecDataMap_t* m) { m_execDataMap = m; }

    void run();

private:
    const ExecDataMap_t * m_execDataMap;
    std::unordered_map<unsigned long, RunStats> m_runstats;
};