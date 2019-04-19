#include "ADOutlier.hpp"

ADOutlier::ADOutlier() : m_execDataMap(nullptr) {

}

ADOutlier::~ADOutlier() {

}

void ADOutlier::run() {
    if (m_execDataMap == nullptr) return;
    std::cout << "Run" << std::endl;

    std::unordered_map<unsigned long, RunStats> temp;
    for (auto it : *m_execDataMap) {
        // std::cout << it.first << ": " << it.second.size() << std::endl;
        for (auto itt : it.second) {
            // std::cout << itt->get_runtime() << std::endl;
            temp[it.first].add(static_cast<double>(itt->get_runtime()));
        }
        std::cout << temp[it.first] << std::endl;
    }


    // todo: update temp runstats

    // 

}