#include "ADio.hpp"

ADio::ADio(IOMode mode) : m_mode(mode), m_execWindow(5) {
    m_file.open("/home/sungsooha/Desktop/CODAR/PerformanceAnalysis/cpp/test.txt", std::ios::app);
    if (!m_file.is_open()) {
        std::cerr << "Cannot open file: " << std::endl;
        exit(EXIT_FAILURE);
    }
    m_q = new DispatchQueue("ADio", 1);
}

ADio::~ADio() {
    delete m_q;
}

void ADio::write(CallListMap_p_t* m) {
    std::ofstream& file = m_file;
    m_q->dispatch([&file, m]{
        for (auto it_p : *m) {
            for (auto it_r : it_p.second) {
                for (auto it_t: it_r.second) {
                    CallList_t& cl = it_t.second;
                    for (auto it : cl) {
                        file << it << std::endl;
                    }
                }
            }
        }
        delete m;
    });
}