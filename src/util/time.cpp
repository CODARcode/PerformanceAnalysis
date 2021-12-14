#include <chimbuko/util/time.hpp>
#include <iomanip>
#include <ctime>
#include <sstream>

namespace chimbuko{

  std::string getDateTime(){
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y/%m/%d %H:%M:%S");
    return ss.str();
  }

  std::string getDateTimeFileExt(){
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y.%m.%d-%H.%M.%S");
    return ss.str();
  }

  Timer::Timer(bool start_now) : m_running(false), m_add(Clock::duration::zero()){
    if(start_now) start();
  }

  void Timer::start(){
    m_start = Clock::now();
    m_add = Clock::duration::zero();
    m_running = true;
  }

  void Timer::pause(){
    if(m_running){
      Clock::time_point now = Clock::now();    
      m_add += now - m_start;
      m_running = false;
    }
  }

  void Timer::unpause(){
    assert(!m_running);
    m_start = Clock::now();
    m_running = true;
  }

  double Timer::elapsed_us() const{
    if(m_running){
      Clock::time_point now = Clock::now();
      return std::chrono::duration_cast<MicroSec>(now - m_start + m_add).count();
    }else return std::chrono::duration_cast<MicroSec>(m_add).count();
  }

  double Timer::elapsed_ms() const{
    return elapsed_us()/1000.;
  }


};
