#pragma once

#include "chimbuko/ad/ADEvent.hpp"

namespace chimbuko{

  /**
   * @brief A class that contains information about the anomalies captured by the AD
   */
  class Anomalies{
  public:
    /**
     * @brief Insert a detected outlier
     */
    void insert(CallListIterator_t outlier);

    /**
     * @brief Get the outliers associated with a given function
     */
    const std::vector<CallListIterator_t> & funcOutliers(const unsigned long func_id) const;

    /**
     * @brief Get all outliers
     */
    const std::vector<CallListIterator_t> & allOutliers() const{ return m_all_outliers; }

    /**
     * @brief Get number of outliers associated with a given function
     */
    size_t nFuncOutliers(const unsigned long func_id) const{ return funcOutliers(func_id).size();  }

    /**
     * @brief Get number of outliers
     */
    size_t nOutliers() const { return m_all_outliers.size(); }
      
    
  private:
    std::vector<CallListIterator_t> m_all_outliers; /**< Array of outliers */
    std::unordered_map<unsigned long, std::vector<CallListIterator_t> > m_func_outliers; /**< Map of function index to associated outliers */
    
  };





};
