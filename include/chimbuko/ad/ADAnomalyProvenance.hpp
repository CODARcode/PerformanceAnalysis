#pragma once

#include <chimbuko/ad/ADEvent.hpp>
#include <chimbuko/param.hpp>
#include "chimbuko/ad/ADCounter.hpp"
#include "chimbuko/ad/ADMetadataParser.hpp"
#include "chimbuko/ad/ADNormalEventProvenance.hpp"
#include "chimbuko/util/Anomalies.hpp"
#include "chimbuko/util/PerfStats.hpp"

namespace chimbuko{

  /**
   * @brief A class that gathers provenance data associated with a detected anomaly
   */
  class ADAnomalyProvenance{
  public:
    /**
     * @brief Extract the provenance information and store in internal JSON fields
     * @param call The anomalous execution
     * @param event_man The event manager
     * @param algo_params The algorithm parameters that resulted in the event classification
     * @param counters The counter manager
     * @param metadata The metadata manager
     * @param window_size The number of events (on this process/rank/thread) either side of the anomalous event that are captured in the window field
     * @param io_step Index of io step
     * @param io_step_tstart Timestamp of beginning of io frame
     * @param io_step_tend Timestamp of end of io frame
     */     
    ADAnomalyProvenance(const ExecData_t &call,
			const ADEvent &event_man, //for stack trace
			const ParamInterface &algo_params, //algorithm parameters
			const ADCounter &counters, //for counters
			const ADMetadataParser &metadata, //for env information including GPU context/device/stream
			const int window_size,
			const int io_step, 
			const unsigned long io_step_tstart, const unsigned long io_step_tend);

    /**
     * @brief Serialize anomaly data into JSON construct
     */
    nlohmann::json get_json() const;
    
    /**
     * @brief Extract the json provDB entries for the anomalies and normal events from an Anomalies collection
     *
     * @param anom_event_entries The provDB entries for anomalous events
     * @param normal_event_entries The provDB entries for normal events
     * @param normal_event_manager An instance of ADNormalEventProvenance that maintains normal events between calls
     * @param perf Performance timing
     * @param anomalies The Anomalies object containing anomalies and select normal events for this io step
     * @param step The io step
     * @param first_event_ts The timestamp of the first event in the io step
     * @param last_event_ts The timestamp of the last event in the io step
     * @param anom_win_size The window size of events to capture around each anomaly
     * @param algo_params The outlier algorithm parameters
     * @param event_man The event manager object
     * @param counters The counter manager object
     * @param metadata The metadata manager object
     */
    static void getProvenanceEntries(std::vector<nlohmann::json> &anom_event_entries,
				     std::vector<nlohmann::json> &normal_event_entries,
				     ADNormalEventProvenance &normal_event_manager,
				     PerfStats &perf,
				     const Anomalies &anomalies,
				     const int step,
				     const unsigned long first_event_ts,
				     const unsigned long last_event_ts,
				     const unsigned int anom_win_size,
				     const ParamInterface &algo_params,
				     const ADEvent &event_man,
				     const ADCounter &counters, 
				     const ADMetadataParser &metadata);

    /**
     * @brief Extract the json provDB entries for the anomalies and normal events from an Anomalies collection
     *
     * @param anom_event_entries The provDB entries for anomalous events
     * @param normal_event_entries The provDB entries for normal events
     * @param normal_event_manager An instance of ADNormalEventProvenance that maintains normal events between calls
     * @param anomalies The Anomalies object containing anomalies and select normal events for this io step
     * @param step The io step
     * @param first_event_ts The timestamp of the first event in the io step
     * @param last_event_ts The timestamp of the last event in the io step
     * @param anom_win_size The window size of events to capture around each anomaly
     * @param algo_params The outlier algorithm parameters
     * @param event_man The event manager object
     * @param counters The counter manager object
     * @param metadata The metadata manager object
     */
    static void getProvenanceEntries(std::vector<nlohmann::json> &anom_event_entries,
				     std::vector<nlohmann::json> &normal_event_entries,
				     ADNormalEventProvenance &normal_event_manager,
				     const Anomalies &anomalies,
				     const int step,
				     const unsigned long first_event_ts,
				     const unsigned long last_event_ts,
				     const unsigned int anom_win_size,
				     const ParamInterface &algo_params,
				     const ADEvent &event_man,
				     const ADCounter &counters, 
				     const ADMetadataParser &metadata){
      PerfStats perf;
      getProvenanceEntries(anom_event_entries, normal_event_entries,
			   normal_event_manager, perf, anomalies, step, first_event_ts, last_event_ts,
			   anom_win_size, algo_params, event_man, counters, metadata);
    }		     

  private:
    /**
     * @brief Get the call stack
     */
    void getStackInformation(const ExecData_t &call, const ADEvent &event_man);
    
    /**
     * @brief Get counters in execution window
     */
    void getWindowCounters(const ExecData_t &call);

    /**
     * @brief Determine if it is a GPU event, and if so get the context
     */
    void getGPUeventInfo(const ExecData_t &call, const ADEvent &event_man, const ADMetadataParser &metadata);

    /**
     * @brief Get the execution window
     * @param window_size The number of events either side of the call that are captured
     */
    void getExecutionWindow(const ExecData_t &call,
			    const ADEvent &event_man,
			    const int window_size);


    ExecData_t m_call; /**< The anomalous event*/
    std::vector<nlohmann::json> m_callstack; /**< Call stack from function back to root. Each entry is the function index and name */
    nlohmann::json m_algo_params; /**< JSON object containing the algorithm parameters used to classify the anomaly*/
    std::vector<nlohmann::json> m_counters; /**< A list of counter events that occurred during the execution of the anomalous function*/
    bool m_is_gpu_event; /**< Is this an anomaly that occurred on a GPU? */
    nlohmann::json m_gpu_location; /**< If it was a GPU event, which device/context/stream did it occur on */
    nlohmann::json m_gpu_event_parent_info; /**< If a GPU event, info related to CPU event spawned it (name, thread, callstack) */
    nlohmann::json m_exec_window; /**< Window of function executions and MPI commuinications around anomalous execution*/
    int m_io_step; /**< IO step*/
    unsigned long m_io_step_tstart; /**< Timestamp of start of io step*/
    unsigned long m_io_step_tend; /**< Timestamp of end of io step*/
  };


};
