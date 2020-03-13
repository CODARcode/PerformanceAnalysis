#pragma once

#include "chimbuko/AD.hpp"

namespace chimbuko {

  class Chimbuko{
  public:
    Chimbuko();
    ~Chimbuko();

    void init_io(int rank, IOMode mode, std::string outputPath, 
		 std::string addr, unsigned int winSize=0);
    void init_parser(std::string data_dir, std::string inputFile, std::string engineType);
    void init_event(bool verbose=false);
    void init_outlier(int rank, double sigma, std::string addr="");

    void finalize();

    bool use_ps() const { return m_outlier->use_ps(); }
    void show_status(bool verbose=false ) const { m_event->show_status(verbose); }
    bool get_status() const { return m_parser->getStatus(); }
    int get_step() const { return m_parser->getCurrentStep(); }

    void run(int rank, 
	     unsigned long long& n_func_events, 
	     unsigned long long& n_comm_events,
	     unsigned long& n_outliers,
	     unsigned long& frames,
#ifdef _PERF_METRIC
	     std::string perf_outputpath="",
	     int         perf_step=10,
#endif
	     bool only_one_frame=false,
	     int interval_msec=0);

  private:
    /**
     * @brief Signal the parser to parse the adios2 timestep
     * @param[out] step index
     * @param[out] number of func events parsed
     * @param[out] number of comm events parsed
     * @return false if unsuccessful, true otherwise
     */
    bool parseInputStep(int &step, 
			unsigned long long& n_func_events, 
			unsigned long long& n_comm_events);


    /*
     * @brief Extract parsed events and insert into the event manager
     * @param rank The MPI rank of the process
     * @param step The adios2 stream step index
     */
    void extractEvents(int rank, int step);
  
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
