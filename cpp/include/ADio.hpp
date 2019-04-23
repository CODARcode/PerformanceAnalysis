#pragma once
#include "ADDefine.hpp"
#include "ADEvent.hpp"

class ADio {

public:
    ADio(IOMode mode=IOMode::Offline);
    ~ADio();


    // void read();
    void write(CallListMap_p_t* m);

private:
    IOMode m_mode;    
    unsigned int m_execWindow;
};