#pragma once
#include <chimbuko_config.h>
#include <chimbuko/modules/performance_analysis/ad/ADEvent.hpp>
#include <chimbuko/core/param.hpp>
#include "chimbuko/modules/performance_analysis/ad/ADCounter.hpp"
#include "chimbuko/modules/performance_analysis/ad/ADMetadataParser.hpp"
#include "chimbuko/modules/performance_analysis/ad/ADNormalEventProvenance.hpp"
#include "chimbuko/modules/performance_analysis/ad/ADMonitoring.hpp"
#include "chimbuko/core/ad/ADOutlier.hpp"
#include "chimbuko/core/util/PerfStats.hpp"

namespace chimbuko{

  /**
   * @brief A class that gathers provenance data associated with a set of detected anomalies
   */
  class ADAnomalyProvenance{
  public:
    ADAnomalyProvenance(const ADEventIDmap &event_man);

    /**
     * @brief Extract the json provDB entries for the anomalies and normal events from an data interface after running the AD
     *
     * @param anom_event_entries The provDB entries for anomalous events
     * @param normal_event_entries The provDB entries for normal events
     * @param iface The interface object containing references to labeled events on this io step
     * @param step The io step
     * @param first_event_ts The timestamp of the first event in the io step
     * @param last_event_ts The timestamp of the last event in the io step
     */
    void getProvenanceEntries(std::vector<nlohmann::json> &anom_event_entries,
			      std::vector<nlohmann::json> &normal_event_entries,
			      const ADExecDataInterface &iface,
			      const int step,
			      const unsigned long first_event_ts,
			      const unsigned long last_event_ts);

    /**
     * @brief Extract the provenance information for a specific call
     * @param call The call
     * @param step The io step
     * @param first_event_ts The timestamp of the first event in the io step
     * @param last_event_ts The timestamp of the last event in the io step
     *
     * Note the minimum anomaly time for recorded data does not apply to this call
     */
    nlohmann::json getEventProvenance(const ExecData_t &call,
				      const int step,
				      const unsigned long first_event_ts,
				      const unsigned long last_event_ts) const;

    /**
     * @brief Set the number of events on either side of the anomaly to record in the window view (default 5)
     */
    void setWindowSize(const int sz){ m_window_size = sz; }

    /**
     * @brief Set the minimum exclusive runtime (in microseconds) for recorded anomalies (default 0) 
     *
     * Anomalies with exclusive runtime less than this will not have their data recorded
     */
    void setMinimumAnomalyTime(const unsigned long to){ m_min_anom_time = to; }

    /**
     * @brief If linked, performance information will be gathered
     */
    void linkPerf(PerfStats* perf){ m_perf = perf; }

    /**
     * @brief Link the algorithm parameters to have algorithm information included in the output
     */
    void linkAlgorithmParams(ParamInterface const *algo_params){ m_algo_params = algo_params; }

    /**
     * @brief Link the monitoring data to have node state information included in the output
     */
    void linkMonitoring(ADMonitoring const *monitoring){ m_monitoring = monitoring; }
        
    /**
     * @brief Link the metadata owner to have metadata (GPU information, hostname, etc) included in the output
     *
     * This is also necessary to have Chimbuko track back the host-side parent of a GPU kernel
     */
    void linkMetadata(ADMetadataParser const* metadata){ m_metadata = metadata; }


    /**
     * @brief Link the event manager. The event manager pointer is set in the constructor but this function allows it to be changed
     */
    void linkEventManager(ADEventIDmap const *event_man){ m_event_man = event_man; }
    
  private:

    /**
     * @brief Get the call stack
     */
    void getStackInformation(nlohmann::json &into, const ExecData_t &call) const;
    
    /**
     * @brief Get counters in execution window
     */
    void getWindowCounters(nlohmann::json &into, const ExecData_t &call) const;


    /**
     * @brief Determine if it is a GPU event, and if so get the context
     *
     * Requires metadata manager to be linked
     */
    void getGPUeventInfo(nlohmann::json &into, const ExecData_t &call) const;


    /**
     * @brief Get the execution window
     */
    void getExecutionWindow(nlohmann::json &into, const ExecData_t &call) const;

    /**
     * @brief Get the node state data
     *
     * Requires the monitoring class to be linked
     */
    void getNodeState(nlohmann::json &into) const;

    /**
     * @brief Get the hostname metadata
     *
     * Requires metadata manager to be linked
     */
    void getHostname(nlohmann::json &into) const;

    
    /**
     * @brief Get the algorithm parameters
     *
     * Requires the algorithm parameters manager to be linked
     */
    void getAlgorithmParams(nlohmann::json &into, const ExecData_t &call) const;


    PerfStats *m_perf; /**< Maintain performance timings*/
    ADEventIDmap const *m_event_man; /**< Contains the map between event index and event*/
    ADMetadataParser const *m_metadata; /**< Contains metadata information*/
    int m_window_size; /**< The number of events either side of the anomaly to capture in the window*/
    ADMonitoring const *m_monitoring; /**< Node state information from TAU's monitoring plugin*/
    ParamInterface const *m_algo_params; /**< The algorithm parameters*/
    unsigned long m_min_anom_time; /**< Anomalies with exclusive runtime less than this will not have their data recorded*/

    ADNormalEventProvenance m_normalevents; /**< Maintain information on a selection of normal events*/
  };

};
