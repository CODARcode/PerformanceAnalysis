#include "ADio.hpp"

ADio::ADio(IOMode mode) : m_mode(mode), m_execWindow(5) {

}

ADio::~ADio() {

}

void ADio::write(CallListMap_p_t* m) {
    // for (auto it_p : *m) {
    //     for (auto it_r : it_p.second) {
    //         for (auto it_t: it_r.second) {
    //             CallList_t& cl = it_t.second;
    //         }
    //     }
    // }

    delete m;
}