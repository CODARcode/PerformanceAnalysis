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
        
        std::cout << "Function " << it.first << std::endl;
        std::vector<double> data;
        for (auto itt : it.second) {
            // std::cout << itt->get_runtime() << std::endl;
            temp[it.first].push(static_cast<double>(itt->get_runtime()));
            data.push_back(static_cast<double>(itt->get_runtime()));
        }
        std::cout << "RunStats: " << temp[it.first];
        std::cout << "Static  : " << "n: " << data.size()
                                  << ", mean: " << static_mean(data)
                                  << ", std: " << static_std(data) << std::endl;

        RunStats rs = temp[it.first];    
        for (int i = 0; i < 10000000; i++) {
            // for (auto itt : it.second) {
            //     data.push_back(static_cast<double>(itt->get_runtime()));
            // }
            rs += temp[it.first];
        }
        std::cout << "RunStats: " << rs;
        // std::cout << "Static  : " << "n: " << data.size()
        //                           << ", mean: " << static_mean(data)
        //                           << ", std: " << static_std(data) << std::endl;
    }


    // todo: update temp runstats (parameter server)

    // todo: Sstd algorithm

    // todo: update # abnormal?

}