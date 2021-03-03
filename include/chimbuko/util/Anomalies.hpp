#pragma once

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
     * @brief Insert used in HBOS
     */
    void insert(CallListIterator_t event, EventType type, double hbos_score, double threshold);

    const std::unordered_map<unsigned long, std::vector<std::vector<double> > > & allHbosScores() const{return m_func_outliers_hbos_scores_and_threshold;}

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

    std::unordered_map<unsigned long, std::vector<std::vector<double> > (2)> m_func_outliers_hbos_scores_and_threshold; /**< Map of function index to a 2D vector where index 0: HBOS scores, index 1: threshold */
  };





};
