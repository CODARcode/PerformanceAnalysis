#pragma once
#include <chimbuko_config.h>
#include "chimbuko/core/ad/ADOutlier.hpp"
#include <chimbuko/modules/performance_analysis/ad/ADEvent.hpp>

namespace chimbuko {

  /**
   * @brief An implementation of the data set interface for ExecData
   */
  class ADExecDataInterface: public ADDataInterface{
  public:
    /**
     * @brief Enumeration of which statistic is used for outlier detection
     */
    enum OutlierStatistic { ExclusiveRuntime, InclusiveRuntime };

    ADExecDataInterface(ExecDataMap_t const* execDataMap, OutlierStatistic stat = ExclusiveRuntime);

    /**
     * @brief Set the statistic used for the anomaly detection
     */
    void setStatistic(OutlierStatistic to){ m_statistic = to; }

    /**
     * @brief Extract the appropriate statistic from an ExecData_t object
     */
    double getStatisticValue(const ExecData_t &e) const;

    /**
     * @brief Return true if the specified function is being ignored
     */
    bool ignoringFunction(const std::string &func) const;

    /**
     * @brief Set a function to be ignored by the outlier detection.
     *
     * All such events are flagged as normal
     */
    void setIgnoreFunction(const std::string &func);

    /**
     * @brief Retrieve the ExecData entry associated with a particular element
     */
    CallListIterator_t getExecDataEntry(size_t dset_index, size_t elem_index) const;
    
    std::vector<ADDataInterface::Elem> getDataSet(size_t dset_index) const override;

    /**
     * @brief Record the labels and scores of data points associated with data set index 'dset_index' in implementation-internal format
     */
    void recordDataSetLabelsInternal(const std::vector<Elem> &data, size_t dset_index) override;
    
    /**
     * @brief Return the function index associated with a given data set
     */
    size_t getDataSetModelIndex(size_t dset_index) const override{ return m_dset_fid_map[dset_index]; }

    /**
     * @brief Return the data set index associated with a given function index
     * 
     * Not optimized, primarily for testing
     */
    size_t getDataSetIndexOfFunction(size_t fid) const;

    /**
     * @brief Tell the interface to ignore the first call to a function on a given pid/rid/tid
     * @param functions_seen A pointer to a FunctionsSeenType instance where the functions that have been seen will be recorded
     */
    typedef std::unordered_set< std::array<unsigned long, 4>, ArrayHasher<unsigned long,4> > FunctionsSeenType;
    void setIgnoreFirstFunctionCall(FunctionsSeenType *functions_seen){ m_ignore_first_func_call = true; m_local_func_exec_seen = functions_seen; }

  private:
    OutlierStatistic m_statistic; /** Which statistic to use for outlier detection */
    std::unordered_set<std::string> m_func_ignore; /**< A list of functions that are ignored by the anomaly detection (all flagged as normal events)*/
    ExecDataMap_t const* m_execDataMap;     /**< execution data map */
    std::vector<size_t> m_dset_fid_map; /**< Map of data set index to func idx*/

    bool m_ignore_first_func_call;
    FunctionsSeenType *m_local_func_exec_seen; /**< Map(program id, rank id, thread id, func id) exist if previously seen*/
    mutable std::unordered_map<size_t, std::vector<ADDataInterface::Elem> > m_dset_cache; /** Cache previously-generated datasets to avoid extra work and ensure the same results for sucessive calls */ 
  };

};
