#pragma once
#include <chimbuko_config.h>
#include "chimbuko/ad/ADEvent.hpp"

namespace chimbuko{

  /**
   * @brief A class that contains information about the anomalies captured by the AD.
   * Also stored are a few examples of normal executions, allowing for comparison with outliers
   */
  class Anomalies{
  public:
    enum class EventType { Outlier, Normal };

    Anomalies(): m_n_events_total(0){}

    /**
     * @brief Record an anomalous event
     */
    void recordAnomaly(CallListIterator_t event);

    /**
     * @brief Record a normal event if it has the lowest score for its function
     * @return true if the event was recorded, false otherwise
     */
    bool recordNormalEventConditional(CallListIterator_t event);
   
    /**
     * @brief Get all recorded outliers/normal events
     */
    const std::vector<CallListIterator_t> & allEventsRecorded(EventType type) const{ return type == EventType::Outlier ? m_all_outliers : m_all_normal_execs; }

    /**
     * @brief Get number of outliers/normal events recorded
     *
     * Note: This is not all of the normal events, only the selection of normal events that we keep for comparison purposes
     */
    size_t nEventsRecorded(EventType type) const { return allEventsRecorded(type).size(); }

    /**
     * @brief  Get the total number of events analyzed (both recorded and unrecorded)
     */
    size_t nEvents() const{ return m_n_events_total; }

    /**
     * @brief Count the number of events recorded of a given type for a particular function
     */
    size_t nFuncEventsRecorded(unsigned long fid, EventType type) const;

    /**
     * @brief Return an array of iterators to events of the given type associated with a specific function
     */
    std::vector<CallListIterator_t> funcEventsRecorded(unsigned long fid, EventType type) const;

  private:
    std::vector<CallListIterator_t> m_all_outliers; /**< Array of outliers */
    std::vector<CallListIterator_t> m_all_normal_execs; /**< Array of normal executions (the algorithm will capture a limited number of these for comparison with outliers)*/
    std::unordered_map<unsigned long, size_t> m_func_normal_exec_idx; /**< Map of function index to the index of the array entry containing the normal execution recorded for that function*/

    size_t m_n_events_total; /**< Total number of events analyzed (both recorded and unrecorded)*/
  };

};
