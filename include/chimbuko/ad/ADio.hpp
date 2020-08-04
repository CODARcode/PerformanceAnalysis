#pragma once
#include "chimbuko/ad/ADDefine.hpp"
#include "chimbuko/ad/ADEvent.hpp"
#include "chimbuko/ad/ADCounter.hpp"
#include "chimbuko/util/DispatchQueue.hpp"
#include <fstream>
#include <curl/curl.h>

namespace chimbuko {

  /**
   * @brief A class that manages communication of JSON-formatted data to the parameter server via CURL and/or to disk
   *
   * The output type is optional: If a URL is provided, the data will be sent via CURL, and if a path is provided it will be written to disk. If both are provided both methods will be used.
   */
  class ADio {
  public:
    ADio();
    ~ADio();

    /**
     * @brief Set the MPI rank of the current process
     */
    void setRank(int rank) { m_rank = rank; }

    /**
     * @brief Get the MPI rank of the current process
     */
    int getRank() const { return m_rank; }

    /**
     * @brief Initialize curl and set the URL of the server
     * @param url The URL of the server
     * @return true if curl could be initialized, false otherwise
     *
     * Note this function does not test that connection could be established to the server
     */
    bool open_curl(std::string url);

    /**
     * @brief Finalize curl
     */
    void close_curl();

    /**
     * @brief For disk output, provide the write path
     *
     * A zero length string will disable disk IO
     */
    void setOutputPath(std::string path);

    /**
     * @brief Get the write path
     */
    std::string getOutputPath() const { return m_outputPath; }

    /**
     * @brief If a DispatchQueue instance has not previously been created, create an instance with the parameters provided
     */
    void setDispatcher(std::string name="ioDispatcher", size_t thread_cnt=1);

    /**
     * @brief Set the size of the window of events captured around every anomaly
     */
    void setWinSize(unsigned int winSize) { m_execWindow = winSize; }

    /**
     * @brief Get the size of the window of events captured around every anomaly
     */
    unsigned int getWinSize() const { return m_execWindow; }

    /**
     * @brief Get the pointer to the CURL instance
     */
    CURL* getCURL() { return m_curl; }

    /**
     * @brief Get the URL of the server (if any)
     */
    std::string getURL() { return m_url; }

    /**
     * @brief Get the number of threads performing the IO
     */
    size_t getNumIOJobs() const {
      if (m_dispatcher == nullptr) return 0;
      return m_dispatcher->size();
    }

    /**
     * @brief Write anomalous events discovered during timestep
     * @param m Organized list of anomalous events
     * @param step adios2 io step
     */
    IOError write(CallListMap_p_t* m, long long step);

    /**
     * @brief Write counter data
     * @param counterList List of counter events
     * @param adios2 io step
     */
    IOError writeCounters(CounterDataListMap_p_t* counterList, long long step);

    /**
     * @brief Write metadata accumulated during this IO step
     * @param newMetadata  Vector of MetaData_t instances containing metadata accumulated during this IO step
     * @param adios2 io step
     */    
    IOError writeMetaData(const std::vector<MetaData_t> &newMetadata, long long step);
    
    /**
     * @brief Set the amount of time between completion of thread dispatcher tasks and destruction of the dispatcher in the class destructor
     * @param secs The time in seconds
     */
    void setDestructorThreadWaitTime(const int secs){ destructor_thread_waittime = secs; }
    
  private:
    unsigned int m_execWindow; /**< Size of window of events captured around anomalous event*/
    std::string m_outputPath; /**< Disk path of written output*/
    DispatchQueue * m_dispatcher; /**< Instance of multi-threaded writer*/
    CURL* m_curl; /**< A pointer to the CURL instance*/
    std::string m_url; /**< The URL of the server*/
    int m_rank; /**< The MPI rank of the current process*/
    int destructor_thread_waittime; /**< Choose thread wait time in seconds after threadhandler has completed (default 10s)*/
  };

} // end of AD namespace
