#pragma once
#include "ADDefine.hpp"
#include "ADEvent.hpp"
#include "DispatchQueue.hpp"
#include <fstream>

class ADio {

public:
    ADio(IOMode mode=IOMode::Offline);
    ~ADio();


    // void read();
    void write(CallListMap_p_t* m);

private:
    IOMode m_mode;    
    unsigned int m_execWindow;
    std::ofstream m_file;
    DispatchQueue * m_q;
};