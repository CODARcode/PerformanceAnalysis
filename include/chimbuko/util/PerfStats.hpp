#pragma once

#ifdef _PERF_METRIC
#include <chrono>
#include <sstream>
#include <unordered_map>
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
    PerfTimer(bool start_now = true);

    /**
     * @brief (Re)start the timer
     */
    void start();
    /**
     * @brief Compute the elapsed time in microseconds since start
     */
    double elapsed_us() const;
    /**
     * @brief Compute the elapsed time in milliconds since start
     */
    double elapsed_ms() const;
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
    /**
     * @brief Construct with empty path and filename (no output will be written unless these are set)
     */
    PerfStats();

    PerfStats(const std::string &output_path, const std::string &filename);

    /*
     * @brief Add a key/value pair to the accumulated statistics
     */
    void add(const std::string &label, const double value);

    /**
     * @brief Set the output path and file name
     */
    void setWriteLocation(const std::string &output_path, const std::string &filename);

    /**
     * @brief Write the running statistics to the file. Only writes out if a path and filename have been provided.
     */
    void write() const;
    
    /**
     * @brief Combine the statistics with another
     */
    PerfStats & operator+=(const PerfStats &r);
  };



  /**
   * @brief A class for storing and writing periodic data, eg memory usage, outstanding provDB requests.
   * It stores and writes only if _PERF_METRIC is active, otherwise it does nothing
   */
  class PerfPeriodic{
#ifdef _PERF_METRIC
    std::string m_outputpath;
    std::string m_filename;
    std::unordered_map<std::string, std::string> m_data;
    bool m_first_write; /**< Is this the first write?*/
#endif
  public:    
    /**
     * @brief Construct with empty path and filename (no output will be written unless these are set)
     */
    PerfPeriodic();

    PerfPeriodic(const std::string &output_path, const std::string &filename);

    /*
     * @brief Add a key/value pair. Any existing value with this label will be overwritten.
     */
    template<typename T>
    void add(const std::string &label, const T &value){
#ifdef _PERF_METRIC
      std::stringstream ss; ss << value; 
      m_data[label] = ss.str();
#endif
    }

    /*
     * @brief Add a key/value pair. Any existing value with this label will be overwritten.
     */
    void add(const std::string &label, const std::string &value){
#ifdef _PERF_METRIC
      m_data[label] = value;
#endif
    }

    /**
     * @brief Set the output path and file name
     */
    void setWriteLocation(const std::string &output_path, const std::string &filename);

    /**
     * @brief Write the running statistics to the file. Only writes out if a path and filename have been provided. After writing, stored values are purged.
     */
    void write();

  };


};
