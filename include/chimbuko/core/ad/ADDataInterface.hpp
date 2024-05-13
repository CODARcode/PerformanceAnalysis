#pragma once
#include <chimbuko_config.h>
#include <vector>

namespace chimbuko {

  /**
   * An abstract interface for passing data to the AD algorithms and recording the results
   * WARNING:  These objects are supposed to be temporary accessors and should be regenerated anew for new data
   * Note: the index here is always 0...Ndataset
   */
  class ADDataInterface{
  public:
    enum class EventType { Outlier, Normal, Unassigned };

    /**
     * @brief Lightweight structure for passing data and results in/out of the AD implementation
     */
    struct Elem{
      double value; /**< The value of the data point */
      size_t index; /**< An arbitrary but unique identifier index for this element*/
      
      EventType label; /**< Label, assigned by AD alg*/
      double score; /**< Outlier score, assigned by AD alg*/

      Elem(double v, size_t i): value(v), index(i), label(EventType::Unassigned), score(-1){}
      inline bool operator==(const Elem &r) const{ return value == r.value && index == r.index && label == r.label && score == r.score; }
    };

    /**
     * @brief Anomaly information for a given data set
     */
    class DataSetAnomalies{
      std::vector<Elem> m_anomalies; /**< Recorded anomalies*/
      std::vector<Elem> m_normal_events; /**< The normal event (array max size 1 !) if present*/
      size_t m_labeled_events; /**< The number of events that were labeled, whether recorded or not*/
      
    public:
      DataSetAnomalies(): m_labeled_events(0){}

      /**
       * @brief Record an event. For normal events this will override the existing one (if present) if the associated score is lower (more normal)
       */
      void recordEvent(const Elem &event);

      /**
       * @brief Get number of outliers/normal events recorded
       *
       * Note: This is not all of the normal events, only the selection of normal events that we keep for comparison purposes
       */
      size_t nEventsRecorded(EventType type) const{ return type == EventType::Outlier ? m_anomalies.size() : m_normal_events.size(); }

      /**
       * @brief  Get the total number of events analyzed (both recorded and unrecorded)
       */
      size_t nEvents() const{ return m_labeled_events; }

      /**
       * @brief Return an array of indices corresponding to events of the given type
       */
      const std::vector<Elem> & getEventsRecorded(EventType type) const{ return type == EventType::Outlier ? m_anomalies : m_normal_events; }     
    };
    
    ADDataInterface(size_t ndataset): m_dset_anom(ndataset){}

    /**
     * @brief Return the number of data sets
     */
    inline size_t nDataSets() const{ return m_dset_anom.size(); }
   
    /**
     * @brief Return the set of data points associated with data set index 'dset_index'
     *
     * These should only include data not previously labeled
     */
    virtual std::vector<Elem> getDataSet(size_t dset_index) const = 0;

    /**
     * @brief Record the labels and scores of data points associated with data set index 'dset_index' in implementation-internal format
     */
    virtual void recordDataSetLabelsInternal(const std::vector<Elem> &data, size_t dset_index) = 0;
    
    /**
     * @brief Record the labels and scores of data points associated with data set index 'dset_index'
     * This function is called by the AD algorithms and records some metadata as well as calling recordDataSetLabelsInternal internally
     */
    void recordDataSetLabels(const std::vector<Elem> &data, size_t dset_index);

    /**
     * @brief Get the results for a particular dataset in a common format
     */
    const DataSetAnomalies & getResults(size_t dset_index) const{ return m_dset_anom[dset_index]; }

    /**
     * @brief  Get the total number of events analyzed (both recorded and unrecorded) summed over all data sets
     */
    size_t nEvents() const;

    /**
     * @brief Return the total number of events recorded of a given type
     */
    size_t nEventsRecorded(EventType type) const;

    /**
     * @brief Return an index that maps a data set index to a unique global identifier for the associated model parameters
     *
     * The number of data sets available in a given batch may vary, and so the data set index is not a good identifier for the model parameters. Instead the implementation should maintain a map of dataset index to model index internally. For example, for function execution data this is the global function id, which is carefully ensured to be consistent between all instances of Chimbuko
     */
    virtual size_t getDataSetModelIndex(size_t dset_index) const = 0;


    virtual ~ADDataInterface(){}

  private:
    std::vector<DataSetAnomalies> m_dset_anom;
  };

};
