#pragma once

#include "chimbuko/AD.hpp"

namespace chimbuko {

class Chimbuko
{
public:
    Chimbuko();
    ~Chimbuko();

    void init_io(int rank, std::string output_dir);
    void init_parser(std::string data_dir, std::string inputFile, std::string engineType);
    void init_event();
    void init_outlier(int rank, double sigma, std::string addr);

    void finalize();

    bool use_ps() const { return m_outlier->use_ps(); }
    void show_status(bool verbose=false ) const { m_event->show_status(verbose); }

    void run(int rank, 
        unsigned long long& n_func_events, 
        unsigned long long& n_comm_events,
        unsigned long& n_outliers,
        unsigned long& frames);

private:
    // int m_rank;
    // std::string m_engineType;  // BPFile or SST
    // std::string m_data_dir;    // *.bp location
    // std::string m_inputFile;   
    // std::string m_output_dir;

    ADParser * m_parser;
    ADEvent * m_event;
    ADOutlierSSTD * m_outlier;
    ADio * m_io;

    // priority queue was used to mitigate a minor problem from TAU plug-in code
    // (e.g. pthread_create in wrong position). Actually, it doesn't casue 
    // call stack violation and the priority queue approach is not the ultimate
    // solution. So, it is deprecated and hope the problem is solved in TAU side.
    // PQUEUE pq;
};

}