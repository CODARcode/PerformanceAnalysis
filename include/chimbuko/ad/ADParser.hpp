#pragma once
#include <mpi.h>
#include <adios2.h>
#include <unordered_map>
#include <unordered_set>
#include "chimbuko/ad/ADDefine.hpp"
#include "chimbuko/ad/ExecData.hpp"
#include "ADDefine.hpp"

namespace chimbuko {

  /**
   * @brief parsing performance trace data streamed via ADIOS2
   * 
   */
  class ADParser {

  public:
    /**
     * @brief Construct a new ADParser object
     * 
     * @param inputFile ADIOS2 BP filename
     * @param engineType BPFile or SST
     * @param openTimeoutSeconds Timeout for opening ADIOS2 stream
     */
    ADParser(std::string inputFile, std::string engineType="BPFile", int openTimeoutSeconds = 60);
    /**
     * @brief Destroy the ADParser object
     * 
     */
    ~ADParser();

    /**
     * @brief Get the function hash map (function id --> function name)
     * 
     * @return const std::unordered_map<int, std::string>* function hash map 
     */
    const std::unordered_map<int, std::string>* getFuncMap() const {
      return &m_funcMap;
    }
    /**
     * @brief Get the event type hash map (event type id --> event name)
     * 
     * @return const std::unordered_map<int, std::string>* event type hash map
     */
    const std::unordered_map<int, std::string>* getEventType() const {
      return &m_eventType;
    }

    /**
     * @brief Get the status of this parser
     * 
     * @return true if it is connected with a writer
     * @return false if it is disconnected or there are no available data anymore
     */
    bool getStatus() const { return m_status; }
    /**
     * @brief Get the current step (or frame) number
     * 
     * @return int step number
     */
    int getCurrentStep() const { return m_current_step; }

    /**
     * @brief start fetching next available data
     * 
     * @param verbose true to output additional information
     * @return int current step number
     */
    int beginStep(bool verbose=false);
    /**
     * @brief end current step (or frame), only effect on ADIOS2 SST engine
     * 
     */
    void endStep();

    /**
     * @brief update attributes (or meta data), with ADIOS2 BPFile engine it only fetches
     *        the available attributes one time.
     * 
     */
    void update_attributes();
    /**
     * @brief fetching function (timer) data. Results stored internally and extracted using ADParser::getFuncData
     * 
     * @return ParserError error code
     */
    ParserError fetchFuncData();
    /**
     * @brief fetching communication data. Results stored internally and extracted using ADParser::getCommData
     * 
     * @return ParserError error code
     */
    ParserError fetchCommData();

    /**
     * @brief fetching counter data. Results stored internally and extracted using ADParser::getCounterData
     * 
     * @return ParserError error code
     */
    ParserError fetchCounterData();
    
    /**
     * @brief get pointer to an array of a function event specified by `idx`
     * 
     * @param idx index of a function event
     * @return pointer to a function event array
     */
    const unsigned long* getFuncData(size_t idx) const {
      if (idx >= m_timer_event_count) return nullptr;
      return &m_event_timestamps[idx * FUNC_EVENT_DIM];
    }
    /**
     * @brief Get the number of function events in the current step
     * 
     * @return size_t the number of function events
     */
    size_t getNumFuncData() const { return m_timer_event_count; }
    
    /**
     * @brief get pointer to a communication event array specified by `idx`
     * 
     * @param idx index of a communication event
     * @return pointer to a communication event array
     */
    const unsigned long* getCommData(size_t idx) const {
      if (idx >= m_comm_count) return nullptr;
      return &m_comm_timestamps[idx * COMM_EVENT_DIM];
    }
    /**
     * @brief Get the number of communication events in the current step
     * 
     * @return size_t the number of communication events
     */
    size_t getNumCommData() const { return m_comm_count; }


    /**
     * @brief get pointer to a counter event array specified by `idx`
     * 
     * @param idx index of a counter event
     * @return pointer to a counter event array
     */
    const unsigned long* getCounterData(size_t idx) const {
      if (idx >= m_counter_count) return nullptr;
      return &m_counter_timestamps[idx * COUNTER_EVENT_DIM];
    }
    
    /**
     * @brief Get the number of counter events in the current step
     * 
     * @return size_t the number of counter events
     */
    size_t getNumCounterData() const { return m_counter_count; }

    
    /**
     * @brief Get metadata parsed for the first time during the current step
     */
    const std::vector<MetaData_t> & getNewMetaData() const{ return m_new_metadata; }
    
  private:
    adios2::ADIOS   m_ad;                               /**< adios2 handler */
    adios2::IO      m_io;                               /**< adios2 I/O handler */
    adios2::Engine  m_reader;                           /**< adios2 engine handler */

    std::string m_inputFile;                            /**< adios2 BP filename */
    std::string m_engineType;                           /**< adios2 engine type */

    bool m_status;                                      /**< parser status */                              
    bool m_opened;                                      /**< true if connected to a writer or a BP file */
    bool m_attr_once;                                   /**< true for BP engine */
    int  m_current_step;                                /**< current step */

    std::unordered_set<std::string> m_metadata_seen;    /**< Metadata descriptions that have been seen */
    std::vector<MetaData_t> m_new_metadata;             /**< New metadata that appeared on this step */
    
    std::unordered_map<int, std::string> m_funcMap;     /**< function hash map (function id --> function name) */
    std::unordered_map<int, std::string> m_eventType;   /**< event type hash map (event type id --> event name) */

    size_t m_timer_event_count;                         /**< the number of function events in current step */
    std::vector<unsigned long> m_event_timestamps;      /**< array of all function events in the current step */

    size_t m_comm_count;                                /**< the number of communication events in current step */
    std::vector<unsigned long> m_comm_timestamps;       /**< array of all communication events in the current step */

    size_t m_counter_count;                             /**< the number of counter events in the current step */
    std::vector<unsigned long> m_counter_timestamps;    /**< array of all counter events in the current step */
  };

} // end of AD namespace
