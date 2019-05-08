#include "ADOutlier.hpp"

using namespace AD;

/* ---------------------------------------------------------------------------
 * Implementation of ADOutlier class
 * --------------------------------------------------------------------------- */
ADOutlier::ADOutlier() : m_use_ps(false), m_execDataMap(nullptr) {
}

ADOutlier::~ADOutlier() {
}

void ADOutlier::update_runstats(const std::unordered_map<unsigned long, RunStats>& rs)
{
    if (m_use_ps) {
        std::cout << "Update using the parameter server!" << std::endl;
    }

    else {
        for (auto it: rs) {
            m_runstats[it.first] += it.second;
        }
    }
}

void ADOutlier::update_outlier_stats(const std::unordered_map<unsigned long, unsigned long>& m)
{
    if (m_use_ps) {
        std::cout << "Update anomaly statistics with parameter server" << std::endl;
    }

    else {
        for (auto it : m) {
            m_outliers[it.first] += it.second;
        }
    }
}

unsigned long ADOutlier::run() {
    if (m_execDataMap == nullptr) return 0;

    std::unordered_map<unsigned long, RunStats> temp;
    for (auto it : *m_execDataMap) {        
        for (auto itt : it.second) {
            temp[it.first].push(static_cast<double>(itt->get_runtime()));
        }
    }

    // update temp runstats (parameter server)
    update_runstats(temp);

    // run anomaly detection algorithm
    unsigned long n_outliers = 0;
    std::unordered_map<unsigned long, unsigned long> temp_outliers;
    for (auto it : *m_execDataMap) {
        const unsigned long func_id = it.first;
        const unsigned long n = compute_outliers(func_id, it.second);
        n_outliers += n;
        temp_outliers[func_id] = n;
    }

    // update # anomaly
    update_outlier_stats(temp_outliers);

    return n_outliers;
}



/* ---------------------------------------------------------------------------
 * Implementation of ADOutlierSSTD class
 * --------------------------------------------------------------------------- */
ADOutlierSSTD::ADOutlierSSTD() : ADOutlier(), m_sigma(6.0) {
}

ADOutlierSSTD::~ADOutlierSSTD() {
}

unsigned long ADOutlierSSTD::compute_outliers(
    const unsigned long func_id, std::vector<CallListIterator_t>& data) 
{
    if (m_runstats[func_id].N() < 2) return 0;
    unsigned long n_outliers = 0;

    const double mean = m_runstats[func_id].mean();
    const double std  = m_runstats[func_id].std();

    const double thr_hi = mean + m_sigma * std;
    const double thr_lo = mean - m_sigma * std;

    for (auto itt : data) {
        const double runtime = static_cast<double>(itt->get_runtime());
        int label = (thr_lo > runtime || thr_hi < runtime) ? -1: 1;
        if (label == -1) {
            n_outliers += 1;
            itt->set_label(label);
        }
    }

    return n_outliers;
}