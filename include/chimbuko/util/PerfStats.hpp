#pragma once

#ifdef _PERF_METRIC
#include <chrono>
#include "chimbuko/util/RunMetric.hpp"
#endif

namespace chimbuko{

  /**
   * @brief A timer class that only measures time if _PERF_METRIC compile flag is set
   */
  class PerfTimer{
#ifdef _PERF_METRIC
    typedef std::chrono::high_resolution_clock Clock;
    typedef std::chrono::milliseconds MilliSec;
    typedef std::chrono::microseconds MicroSec;
    Clock::time_point m_start; /**< The start timepoint */
#endif
  public:
    PerfTimer(bool start_now = true){
#ifdef _PERF_METRIC
      if(start_now) 
	m_start = Clock::now();
#endif
    }

    /**
     * @brief (Re)start the timer
     */
    void start(){
#ifdef _PERF_METRIC
      m_start = Clock::now();
#endif
    }
    /**
     * @brief Compute the elapsed time in microseconds since start
     */
    double elapsed_us() const{
#ifdef _PERF_METRIC
      Clock::time_point now = Clock::now();
      return std::chrono::duration_cast<MicroSec>(now - m_start).count();
#else 
      return 0;
#endif
    }
    /**
     * @brief Compute the elapsed time in milliconds since start
     */
    double elapsed_ms() const{
#ifdef _PERF_METRIC
      Clock::time_point now = Clock::now();
      return std::chrono::duration_cast<MilliSec>(now - m_start).count();
#else 
      return 0;
#endif
    }
  };    

    


  /**
   * @brief A class that maintains performance statistics of various aspects of the AD module
   *        It's constituent functions only do anything if _PERF_METRIC flag enabled
   */
  class PerfStats{
#ifdef _PERF_METRIC
    std::string m_outputpath;
    std::string m_filename;
    RunMetric m_perf;
#endif
  public:
    PerfStats()
#ifdef _PERF_METRIC
      : m_filename(""), m_outputpath("")
#endif
    {}
    PerfStats(const std::string &output_path, const std::string &filename)
#ifdef _PERF_METRIC
      : m_filename(filename), m_outputpath(output_path)
#endif
    {}

    /*
     * @brief Add a key/value pair to the accumulated statistics
     */
    void add(const std::string &label, const double value){
#ifdef _PERF_METRIC
      m_perf.add(label, value);
#endif
    }

    /**
     * @brief Set the output path and file name
     */
    void setWriteLocation(const std::string &output_path, const std::string &filename){     
#ifdef _PERF_METRIC
      m_outputpath = output_path;
      m_filename = filename;
#endif
    }

    /**
     * @brief Write the running statistics to the file. Only writes out if a path and filename have been provided.
     */
    void write() const{
#ifdef _PERF_METRIC
      if(m_outputpath.length() > 0 && m_filename.length() > 0)
	m_perf.dump(m_outputpath, m_filename);
#endif
    }

    

  };


};
