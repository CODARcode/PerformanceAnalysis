#include <chimbuko/util/PerfStats.hpp>
#include <chimbuko/util/time.hpp>

namespace chimbuko{


  PerfTimer::PerfTimer(bool start_now){
#ifdef _PERF_METRIC
    if(start_now) 
      m_start = Clock::now();
#endif
  }

  void PerfTimer::start(){
#ifdef _PERF_METRIC
    m_start = Clock::now();
#endif
  }

  double PerfTimer::elapsed_us() const{
#ifdef _PERF_METRIC
    Clock::time_point now = Clock::now();
    return std::chrono::duration_cast<MicroSec>(now - m_start).count();
#else 
    return 0;
#endif
  }

  double PerfTimer::elapsed_ms() const{
#ifdef _PERF_METRIC
    Clock::time_point now = Clock::now();
    return double(std::chrono::duration_cast<MicroSec>(now - m_start).count())/1000.;
#else 
    return 0;
#endif
  }


  PerfStats::PerfStats()
#ifdef _PERF_METRIC
    : m_filename(""), m_outputpath("")
#endif
  {}
  
  PerfStats::PerfStats(const std::string &output_path, const std::string &filename)
#ifdef _PERF_METRIC
    : m_filename(filename), m_outputpath(output_path)
#endif
  {}
  
  void PerfStats::add(const std::string &label, const double value){
#ifdef _PERF_METRIC
    m_perf.add(label, value);
#endif
  }
  
  void PerfStats::setWriteLocation(const std::string &output_path, const std::string &filename){     
#ifdef _PERF_METRIC
    m_outputpath = output_path;
    m_filename = filename;
#endif
  }
  
  void PerfStats::write() const{
#ifdef _PERF_METRIC
    if(m_outputpath.length() > 0 && m_filename.length() > 0)
      m_perf.dump(m_outputpath, m_filename);
#endif
  }

  PerfStats & PerfStats::operator+=(const PerfStats &r){
#ifdef _PERF_METRIC
    m_perf += r.m_perf;
#endif
    return *this;
  }

  


  PerfPeriodic::PerfPeriodic()
#ifdef _PERF_METRIC
    : m_filename(""), m_outputpath(""), m_first_write(true)
#endif
  {}
  
  PerfPeriodic::PerfPeriodic(const std::string &output_path, const std::string &filename)
#ifdef _PERF_METRIC
    : m_filename(filename), m_outputpath(output_path), m_first_write(true)
#endif
  {}
   
  void PerfPeriodic::setWriteLocation(const std::string &output_path, const std::string &filename){     
#ifdef _PERF_METRIC
    m_outputpath = output_path;
    m_filename = filename;
#endif
  }
  
  void PerfPeriodic::write(){
#ifdef _PERF_METRIC
    if(m_outputpath.length() > 0 && m_filename.length() > 0 && m_data.size()){
      std::string file = m_outputpath + "/" + m_filename;
      std::ofstream of(file, std::ofstream::out |  (m_first_write ? std::ostream::trunc : std::ofstream::app) );
      of << getDateTime();
      for(auto const & e : m_data)
	of << " " << e.first << ":" << e.second;
      of << std::endl;
      m_first_write = false; //future writes appended
      m_data.clear(); //purge the data
    }
#endif
  }



};    
