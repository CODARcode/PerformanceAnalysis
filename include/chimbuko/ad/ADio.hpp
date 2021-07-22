#pragma once
#include <chimbuko_config.h>
#include "chimbuko/ad/ADDefine.hpp"
#include "chimbuko/ad/ADEvent.hpp"
#include "chimbuko/ad/ADCounter.hpp"
#include "chimbuko/util/DispatchQueue.hpp"
#include <fstream>

namespace chimbuko {

  /**
   * @brief A class that manages communication of JSON-formatted data to disk
   */
  class ADio {
  public:
    /**
     * @brief Constructor
     * @param program_idx The program index
     * @param rank MPI rank
     */
    ADio(unsigned long program_idx, int rank);

    ~ADio();

    /**
     * @brief Set the MPI rank of the current process
     */
    void setProgramIdx(unsigned long pid) { m_program_idx = pid; }

    /**
     * @ brief Get the program idx
     */
    unsigned long getProgramIdx() const{ return m_program_idx; }

    /**
     * @brief Set the MPI rank of the current process
     */
    void setRank(int rank) { m_rank = rank; }

    /**
     * @brief Get the MPI rank of the current process
     */
    int getRank() const { return m_rank; }

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
     * @brief Get the number of threads performing the IO
     */
    size_t getNumIOJobs() const {
      if (m_dispatcher == nullptr) return 0;
      return m_dispatcher->size();
    }

    /**
     * @brief Write an array of JSON objects
     * @param file_stub File will be ${file_stub}.${step}.json
     */
    IOError writeJSON(const std::vector<nlohmann::json> &data, long long step, const std::string &file_stub);
    
    /**
     * @brief Set the amount of time between completion of thread dispatcher tasks and destruction of the dispatcher in the class destructor
     * @param secs The time in seconds
     */
    void setDestructorThreadWaitTime(const int secs){ destructor_thread_waittime = secs; }
    
  private:
    std::string m_outputPath; /**< Disk path of written output*/
    DispatchQueue * m_dispatcher; /**< Instance of multi-threaded writer*/
    unsigned long m_program_idx;                        /**< Program index*/
    int m_rank; /**< The MPI rank of the current process*/
    int destructor_thread_waittime; /**< Choose thread wait time in seconds after threadhandler has completed (default 10s)*/
  };

} // end of AD namespace
