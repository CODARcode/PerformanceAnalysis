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

    /**
     * @brief Insert a detected outlier/normal execution
     */
    void insert(CallListIterator_t event, EventType type);

    /**
     * @brief Get the outlier/normal events associated with a given function
     */
    const std::vector<CallListIterator_t> & funcEvents(const unsigned long func_id, EventType type) const;

    /**
     * @brief Get all outliers/normal events
     */
    const std::vector<CallListIterator_t> & allEvents(EventType type) const{ return type == EventType::Outlier ? m_all_outliers : m_all_normal_execs; }

    /**
     * @brief Get number of outliers/normal events associated with a given function
     */
    size_t nFuncEvents(const unsigned long func_id, EventType type) const{ return funcEvents(func_id, type).size();  }

    /**
     * @brief Get number of outliers/normal events
     */
    size_t nEvents(EventType type) const { return allEvents(type).size(); }

  private:
    std::vector<CallListIterator_t> m_all_outliers; /**< Array of outliers */
    std::unordered_map<unsigned long, std::vector<CallListIterator_t> > m_func_outliers; /**< Map of function index to associated outliers */

    std::vector<CallListIterator_t> m_all_normal_execs; /**< Array of normal executions (the algorithm will capture a limited number of these for comparison with outliers)*/
    std::unordered_map<unsigned long, std::vector<CallListIterator_t> > m_func_normal_execs; /**< Map of function index to associated normal executions */
  };

};
