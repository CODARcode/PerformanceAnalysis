#pragma once
#include "ADEvent.hpp"
#include "ExecData.hpp"
#include "RunStats.hpp"

class ADOutlier {

public:
    ADOutlier();
    virtual ~ADOutlier();

    void linkExecDataMap(const ExecDataMap_t* m) { m_execDataMap = m; }
    
    unsigned long run();

protected:
    void update_runstats(const std::unordered_map<unsigned long, RunStats>& rs);
    void update_outlier_stats(const std::unordered_map<unsigned long, unsigned long>& m);
    virtual unsigned long compute_outliers(
        const unsigned long func_id, std::vector<CallListIterator_t>& data) = 0;

protected:
    bool m_use_ps;

    const ExecDataMap_t * m_execDataMap;
    std::unordered_map<unsigned long, RunStats> m_runstats;
    std::unordered_map<unsigned long, unsigned long> m_outliers;
};

class ADOutlierSSTD : public ADOutlier {

public:
    ADOutlierSSTD();
    ~ADOutlierSSTD();

protected:
    unsigned long compute_outliers(
        const unsigned long func_id, std::vector<CallListIterator_t>& data);

private:
    double m_sigma;
};