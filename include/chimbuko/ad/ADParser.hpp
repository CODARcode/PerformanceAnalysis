#pragma once
#include <chimbuko_config.h>
#include <adios2.h>
#include <unordered_map>
#include <unordered_set>
#include "ExecData.hpp"
#include "ADDefine.hpp"
#include "ADglobalFunctionIndexMap.hpp"
#include <chimbuko/util/PerfStats.hpp>

namespace chimbuko {

  /**
   * @brief parsing performance trace data streamed via ADIOS2
   *
   * Note: The "function index" assigned to each function by Tau is not necessarily the same for every node as it depends on the order in which the function
   *       is encountered. To deal with this, if the parameter server is running it maintains a global mapping of function name to an index, which is
   *       synchronized to the parser (providing the net client is linked) and the local index is replaced by the global index in the incoming data stream.
   *
   * Note2: The "program index" assigned by Tau is defunct (always 0). We must therefore replace it manually with a correct index to support workflows
   */
  class ADParser {

  public:
    /**
     * @brief Construct a new ADParser object
     *
     * @param inputFile ADIOS2 BP filename
     * @param program_index The index to assign to the program whose trace data is being parsed
     * @param rank Rank of current process
     * @param engineType BPFile or SST
     * @param openTimeoutSeconds Timeout for opening ADIOS2 stream
     */
    ADParser(std::string inputFile, unsigned long program_idx, int rank, std::string engineType="BPFile", int openTimeoutSeconds = 60);
    /**
     * @brief Destroy the ADParser object
     *
     */
    ~ADParser();

    /**
     * @brief Link the net client to the object that maintains a mapping of local function index to global index
     *
     * If this is performed, the parser will replace the local with global index in the incoming data stream
     */
    void linkNetClient(ADThreadNetClient *net_client){ m_global_func_idx_map.linkNetClient(net_client); }

    /**
     * @brief If linked, performance information will be gathered
     */
    void linkPerf(PerfStats* perf){ m_perf = perf; }

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
     * @brief Get the counter hash map (counter id --> counter description)
     *
     * @return const std::unordered_map<int, std::string>* event type hash map
     */
    const std::unordered_map<int, std::string>* getCounterMap() const {
      return &m_counterMap;
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
     *
     * Returns a value of -1 if beginStep has yet to be called.
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

    /**< Set the timeout in seconds on waiting for the next ADIOS2 timestep (default 30s)*/
    void setBeginStepTimeout(int timeout){ m_beginstep_timeout = timeout; }

    /**
     * @brief update attributes (or meta data), with ADIOS2 BPFile engine it only fetches
     *        the available attributes one time.
     *
     */
    void update_attributes();
    /**
     * @brief fetching function (timer) data. Results stored internally and extracted using ADParser::getFuncData
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


    /**
     * @brief Get all the events (func, comm and counter) occuring in the IO step
     *
     * Events are guaranteed ordered by their timestamp on a per-thread basis. Global ordering of events is not guaranteed
     */
    std::vector<Event_t> getEvents() const;


    /**
     * @brief For testing purposes, add the data in the array d to the internal m_event_timestamps array
     * @param d An array of length FUNC_EVENT_DIM
     *
     * Will throw an error if the new array size exceeds the vector capacity as this would invalidate previous Event_t objects
     */
    void addFuncData(unsigned long const* d);

    /**
     * @brief For testing purposes, add the data in the array d to the internal m_counter_timestamps array
     * @param d An array of length COUNTER_EVENT_DIM
     *
     * Will throw an error if the new array size exceeds the vector capacity as this would invalidate previous Event_t objects
     */
    void addCounterData(unsigned long const* d);

    /**
     * @brief For testing purposes, add the data in the array d to the internal m_comm_timestamps array
     * @param d An array of length COMM_EVENT_DIM
     *
     * Will throw an error if the new array size exceeds the vector capacity as this would invalidate previous Event_t objects
     */
    void addCommData(unsigned long const* d);


    /**
     * @brief Set the m_event_timestamps vector capacity in units of FUNC_EVENT_DIM. This will invalidate previous Event_t objects if it requires a realloc!
     */
    void setFuncDataCapacity(size_t cap){ m_event_timestamps.reserve(cap*FUNC_EVENT_DIM); }

    /**
     * @brief Set the m_comm_timestamps vector capacity in units of COMM_EVENT_DIM. This will invalidate previous Event_t objects if it requires a realloc!
     */
    void setCommDataCapacity(size_t cap){ m_comm_timestamps.reserve(cap*COMM_EVENT_DIM); }

    /**
     * @brief Set the m_counter_timestamp vector capacity in units of COUNTER_EVENT_DIM. This will invalidate previous Event_t objects if it requires a realloc!
     */
    void setCounterDataCapacity(size_t cap){ m_counter_timestamps.reserve(cap*COUNTER_EVENT_DIM); }

    /**
     * @brief Set the function index->name map for testing
     */
    void setFuncMap(const std::unordered_map<int, std::string> &m){ m_funcMap = m; }

    /**
     * @brief Set the function event index -> event type  map for testing
     */
    void setEventTypeMap(const std::unordered_map<int, std::string> &m){ m_eventType = m; }

    /**
     * @brief Set the counter index->name map for testing
     */
    void setCounterMap(const std::unordered_map<int, std::string> &m);


    /**
     * @brief Get the global index corresponding to a given local function index. 1<->1 mapping if pserver not connected
     */
    unsigned long getGlobalFunctionIndex(const unsigned long local_idx) const{ return m_global_func_idx_map.lookup(local_idx); }

    /**
     * @brief Get the counter index of the special counter "Correlation ID" if known; else return -1
     */
    int getCorrelationIDcounterIdx() const{ return m_correlation_id_cid; }

    /**
     * @brief When true the parser will override the rank index of the parsed data with the rank member parameter
     *
     * This is useful for example when multiple instances of a non-MPI program are being run and the user wishes to distinguish them by the rank index
     */
    void setDataRankOverride(bool to){ m_data_rank_override = to; }

  private:

    /**
     * @brief Scan the data and check the events are in order
     * @param exit_on_fail Throw an error if the check fails
     * @return true if the events are in order, false otherwise
     */
    bool checkEventOrder(const EventDataType type, bool exit_on_fail) const;

    /**
     * @brief Validate an event to bypass corrupted input data (any event type)
     * @param e Pointer to event data
     */
    bool validateEvent(const unsigned long* e) const;

    /**
     * @brief Create an Event_t instance from the data at the provided pointer and run simple validation
     * @param log_error If true a recoverable error will be logged for invalid events
     */
    std::pair<Event_t,bool> createAndValidateEvent(const unsigned long * data, EventDataType t, size_t idx, const eventID &id, bool log_error = true) const;



    adios2::ADIOS   m_ad;                               /**< adios2 handler */
    adios2::IO      m_io;                               /**< adios2 I/O handler */
    adios2::Engine  m_reader;                           /**< adios2 engine handler */

    std::string m_inputFile;                            /**< adios2 BP filename */
    std::string m_engineType;                           /**< adios2 engine type */

    int m_beginstep_timeout;                            /**< the timeout in seconds on waiting for the next ADIOS2 timestep*/
    bool m_status;                                      /**< parser status */
    bool m_opened;                                      /**< true if connected to a writer or a BP file */
    bool m_attr_once;                                   /**< true for BP engine */
    int  m_current_step;                                /**< current step */
    int  m_rank;                                        /**< Rank of current process */
    unsigned long m_program_idx;                        /**< Program index*/

    std::unordered_set<std::string> m_metadata_seen;    /**< Metadata descriptions that have been seen */
    std::vector<MetaData_t> m_new_metadata;             /**< New metadata that appeared on this step */

    std::unordered_map<int, std::string> m_funcMap;     /**< function hash map (global function id --> function name) */
    std::unordered_map<int, std::string> m_eventType;   /**< event type hash map (event type id --> event name) */
    std::unordered_map<int, std::string> m_counterMap;  /**< counter hash map (counter id --> counter name) */

    int m_correlation_id_cid;                           /**< counter id of "Correlation ID" counters if known (-1 otherwise) */

    size_t m_timer_event_count;                         /**< the number of function events in current step */
    std::vector<unsigned long> m_event_timestamps;      /**< array of all function events in the current step */

    size_t m_comm_count;                                /**< the number of communication events in current step */
    std::vector<unsigned long> m_comm_timestamps;       /**< array of all communication events in the current step */

    size_t m_counter_count;                             /**< the number of counter events in the current step */
    std::vector<unsigned long> m_counter_timestamps;    /**< array of all counter events in the current step */

    ADglobalFunctionIndexMap m_global_func_idx_map;     /**< Maintains mapping of local function index to global function index (if pserver connected) */

    PerfStats* m_perf;                                  /**< Performance monitoring */

    bool m_data_rank_override;                          /**< Overwrite the rank index in the parsed data with member m_rank (default false)*/
  };

} // end of AD namespace
