#pragma once
#include "ADDefine.hpp"
#include "ADEvent.hpp"
#include "DispatchQueue.hpp"
#include <fstream>

namespace AD {

class ADio {
public:
    ADio(IOMode mode=IOMode::Offline);
    ~ADio();

    void setWinSize(unsigned int sz) { m_execWindow = sz; }
    void open();

    // void read();
    void write(CallListMap_p_t* m);

private:
    IOMode m_mode;    
    unsigned int m_execWindow;
    std::fstream m_file;
    DispatchQueue * m_q;
};

} // end of AD namespace