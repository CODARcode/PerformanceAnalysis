#pragma once
#include "chimbuko/ad/ADDefine.hpp"
#include "chimbuko/ad/ADEvent.hpp"
#include "chimbuko/ad/ADCounter.hpp"
#include "chimbuko/util/DispatchQueue.hpp"
#include <fstream>
#include <curl/curl.h>

namespace chimbuko {

  class ADio {
  public:
    ADio();
    ~ADio();

    void setRank(int rank) { m_rank = rank; }
    int getRank() const { return m_rank; }

    bool open_curl(std::string url);
    void close_curl();

    void setOutputPath(std::string path);
    std::string getOutputPath() const { return m_outputPath; }

    void setDispatcher(std::string name="ioDispatcher", size_t thread_cnt=1);

    void setWinSize(unsigned int winSize) { m_execWindow = winSize; }
    unsigned int getWinSize() const { return m_execWindow; }

    CURL* getCURL() { return m_curl; }
    std::string getURL() { return m_url; }

    size_t getNumIOJobs() const {
      if (m_dispatcher == nullptr) return 0;
      return m_dispatcher->size();
    }
    
    IOError write(CallListMap_p_t* m, long long step);

    /**
     * @brief Write counter data
     * @param counterList List of counter events
     * @param adios2 io step
     */
    IOError writeCounters(CounterDataList* counterList, long long step);

    void setDestructorThreadWaitTime(const int secs){ destructor_thread_waittime = secs; }
    
  private:
    void _open(std::fstream& f, std::string filename, IOOpenMode mode);

  private:
    unsigned int m_execWindow;
    std::string m_outputPath;
    DispatchQueue * m_dispatcher;
    CURL* m_curl;
    std::string m_url;
    int m_rank;
    int destructor_thread_waittime; //Choose thread wait time in seconds after threadhandler has completed (default 10s)
  };

} // end of AD namespace
