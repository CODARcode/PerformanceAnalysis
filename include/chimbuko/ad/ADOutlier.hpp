#pragma once
#include <chimbuko_config.h>
#include<array>
#include<unordered_set>
#include "chimbuko/ad/ADEvent.hpp"
#include "chimbuko/ad/ExecData.hpp"
#include "chimbuko/util/RunStats.hpp"
#include "chimbuko/param.hpp"
#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/util/hash.hpp"
#include "chimbuko/ad/ADNetClient.hpp"
#include "chimbuko/util/PerfStats.hpp"
#include "chimbuko/util/Anomalies.hpp"

namespace chimbuko {

  /**
   * An abstract interface for passing data to the AD algorithms and recording the results
   * THIS API IS FOR A SINGLE DATA SET, EG FOR ONE FUNCTION INDEX
   */
  class ADDataSetInterface{
    size_t m_dset_idx; /**< The data set index associated with this interface*/

    std::vector<size_t> m_anomalies; /**< Indices of recorded anomalies*/
    std::vector<size_t> m_normal_events; /**< The normal event (array max size 1 !) if present*/
    double m_normal_event_score; /**< The score of the normal event recorded, if present*/
    size_t m_labeled_events; /**< The number of events that were labeled, whether recorded or not*/

  public:
    enum class EventType { Outlier, Normal };

    struct Elem{
      double value; /**< The value of the data point */
      size_t index; /**< An arbitrary but unique identifier index for this element*/
    };

    ADDataSetInterface(size_t dset_idx): m_dset_idx(dset_idx), m_labeled_events(0){}

    /**
     * @brief Return a unique identifier index associated with this set of data
     *
     * This index is associated with a specific model by the AD algorithms. For example, if analyzing function runtimes, this index could be the function index.
     */
    inline size_t getDataSetIndex() const{ return m_dset_idx; }
    
    /**
     * @brief Return the set of data points
     *
     * These should only include data not previously labeled
     */
    virtual std::vector<Elem> getDataSet() const = 0;

    /**
     * @brief Set the label for the given data point and the associated score
     *
     * This function also records the indices of all anomalies and a single normal event (if detected); that which has the lowest score (most likely) 
     */
    void setDataLabel(size_t index, double score, EventType label);

    /**
     * @brief Apply the label to the data element in whatever its native format. This is called by setDataLabel
     */
    virtual void labelDataElement(size_t index, double score, EventType label) = 0;

    /**
     * @brief Get number of outliers/normal events recorded
     *
     * Note: This is not all of the normal events, only the selection of normal events that we keep for comparison purposes
     */
    size_t nEventsRecorded(EventType type) const{ return type == Outlier ? m_anomalies.size() : m_normal_events.size(); }

    /**
     * @brief  Get the total number of events analyzed (both recorded and unrecorded)
     */
    size_t nEvents() const{ return m_labeled_events; }

    /**
     * @brief Return an array of indices corresponding to events of the given type
     */
    const std::vector<size_t> & getEventsRecorded(EventType type) const{ return type == Outlier ? m_anomalies : m_normal_events; }


    virtual ~ADDataSetInterface(){}
  };



  /**
   * An abstract interface for passing data to the AD algorithms and recording the results
   * THIS API IS FOR A MULTIPLE DATA SETS
   * Note: the index here is always 0...Ndataset
   */
  class ADDataInterface{
    /**
     * @brief Anomaly information for a given data set
     */
    class DataSetAnomalies{
      std::vector<size_t> m_anomalies; /**< Indices of recorded anomalies*/
      std::vector<size_t> m_normal_events; /**< The normal event (array max size 1 !) if present*/
      double m_normal_event_score; /**< The score of the normal event recorded, if present*/
      size_t m_labeled_events; /**< The number of events that were labeled, whether recorded or not*/
      
    public:
      DataSetAnomalies(): m_labeled_events(0){}

      /**
       * @brief Get number of outliers/normal events recorded
       *
       * Note: This is not all of the normal events, only the selection of normal events that we keep for comparison purposes
       */
      size_t nEventsRecorded(EventType type) const{ return type == Outlier ? m_anomalies.size() : m_normal_events.size(); }

      /**
       * @brief  Get the total number of events analyzed (both recorded and unrecorded)
       */
      size_t nEvents() const{ return m_labeled_events; }

      /**
       * @brief Return an array of indices corresponding to events of the given type
       */
      const std::vector<size_t> & getEventsRecorded(EventType type) const{ return type == Outlier ? m_anomalies : m_normal_events; }     
    };
    
    std::vector<DataSetAnomalies> m_dset_anom;
  public:
    enum class EventType { Outlier, Normal };

    struct Elem{
      double value; /**< The value of the data point */
      size_t index; /**< An arbitrary but unique identifier index for this element*/
      Elem(double v, size_t i): value(v), index(i){}
    };

    ADDataInterface(size_t ndataset): m_dset_anom(ndataset){}

    /**
     * @brief Return the number of data sets
     */
    inline size_t nDataSets() const{ m_dset_anom.size(); }
   
    /**
     * @brief Return the set of data points associated with data set index 'dset_index'
     *
     * These should only include data not previously labeled
     */
    virtual std::vector<Elem> getDataSet(size_t dset_index) const = 0;

    /**
     * @brief Set the label for the given data point and the associated score
     *
     * This function also records the indices of all anomalies and a single normal event (if detected); that which has the lowest score (most likely) 
     */
    void setDataLabel(size_t dset_index, size_t elem_index, double score, EventType label);

    /**
     * @brief Apply the label to the data element in whatever its native format. This is called by setDataLabel
     * @param dset_index The index of the dataset  0 <= idx < nDataSets()
     * @param elem_index The index of the element, used as a key so the assigment is implementation dependent
     */
    virtual void labelDataElement(size_t dset_index, size_t elem_index, double score, EventType label) = 0;

    const DataSetAnomalies & getResults(size_t dset_index) const{ return m_dset_anom[dset_index]; }

    /**
     * @brief  Get the total number of events analyzed (both recorded and unrecorded) summed over all data sets
     */
    size_t nEvents() const;

    virtual ~ADDataInterface(){}
  };
  
  /**
   * @brief An implementation of the data set interface for ExecData
   */
  class ADExecDataInterface: public ADDataInterface{
  public:
    /**
     * @brief Enumeration of which statistic is used for outlier detection
     */
    enum OutlierStatistic { ExclusiveRuntime, InclusiveRuntime };

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

    std::vector<ADDataInterface::Elem> getDataSet(size_t dset_index) const override{
      std::vector<ADDataInterface::Elem> out;
      size_t fid = m_dset_fid_map[dset_index];
      auto &data = (*m_execDataMap)[fid];

      if(data.size() == 0) return out;
      const std::string &fname = data.front()->get_funcname();
      bool ignore_func =  ignoringFunction(fname);

      for(size_t i=0;i<data.size();i++){ //loop over events for that function
	auto &e = *data[i];
	if(e.get_label() == 0){ //has not been analyzed previously
	  if(ignore_func) e.set_label(1); //label as normal event
	  else{
	    out.push_back(ADDataInterface::Elem(getStatisticValue(e),i));
	  }
	}
      }
      return out;
    }
    
    void labelDataElement(size_t dset_index, size_t elem_index, double score, EventType label) override{
      auto &e = (*m_execDataMap)[ m_dset_fid_map[dset_index] ][elem_index];
      e.set_outlier_score(score);
      e.set_label( label == ADDataInterface::Outlier ? -1 : 1 );
    }
    
  private:
    OutlierStatistic m_statistic; /** Which statistic to use for outlier detection */
    std::unordered_set<std::string> m_func_ignore; /**< A list of functions that are ignored by the anomaly detection (all flagged as normal events)*/
    const ExecDataMap_t * m_execDataMap;     /**< execution data map */
    std::vector<size_t> m_dset_fid_map; /**< Map of data set index to func idx*/
  };



  /**
   * @brief abstract class for anomaly detection algorithms
   *
   */
  class ADOutlier {

  public:
    /**
     * @brief Enumeration of which statistic is used for outlier detection
     */
    enum OutlierStatistic { ExclusiveRuntime, InclusiveRuntime };

    /**
     * @brief Unified structure for passing the parameters of the AD algorithms to the factory method
     */
    struct AlgoParams{
      OutlierStatistic stat; /**< Which statistic is used */
    
      //SSTD
      double sstd_sigma; /**< The number of sigma that defines an outlier*/
    
      //HBOS/COPOD
      double hbos_thres; /**< The outlier threshold*/
      std::string func_threshold_file; /**< Name of a file containing per-function overrides of the threshold*/

      //HBOS
      bool glob_thres; /**< Should the global threshold be used? */
      int hbos_max_bins; /**< The maximum number of bins in a histogram */

      AlgoParams();
    };


    /**
     * @brief Construct a new ADOutlier object
     *
     */
    ADOutlier(OutlierStatistic stat = ExclusiveRuntime);
    /**
     * @brief Destroy the ADOutlier object
     *
     */
    virtual ~ADOutlier();

    /**
     * @brief Fatory method to select AD algorithm at runtime
     */
    static ADOutlier *set_algorithm(const std::string & algorithm, const AlgoParams &params);

    /**
     * @brief check if the parameter server is in use
     *
     * @return true if the parameter server is in use
     * @return false if the parameter server is not in use
     */
    bool use_ps() const { return m_use_ps; }

    /**
     * @brief copy a pointer to execution data map
     *
     * @param m
     * @see ADEvent
     */
    void linkExecDataMap(const ExecDataMap_t* m) { m_execDataMap = m; }

    /**
     * @brief Link the interface for communicating with the parameter server
     */
    void linkNetworkClient(ADNetClient *client);

    /**
     * @brief abstract method to run the implemented anomaly detection algorithm
     *
     * @param step step (or frame) number
     * @return data structure containing information on captured anomalies
     */
    virtual Anomalies run(int step=0) = 0;

    /**
     * @brief If linked, performance information on the sync_param routine will be gathered
     */
    void linkPerf(PerfStats* perf){ m_perf = perf; }

    /**
     * @brief Get the local copy of the global parameters
     * @return Pointer to a ParamInterface object
     */
    ParamInterface const* get_global_parameters() const{ return m_param; }

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

  protected:
    /**
     * @brief abstract method to compute outliers (or anomalies)
     *
     * @param[out] outliers data structure containing captured anomalies
     * @param func_id function id
     * @param[in,out] data a list of function calls to inspect. Entries will be tagged as outliers
     * @return unsigned long the number of outliers (or anomalies)
     */
    virtual unsigned long compute_outliers(Anomalies &outliers,
					   const unsigned long func_id, std::vector<CallListIterator_t>& data) = 0;

    /**
     * @brief Synchronize the local model with the global model
     *
     * @param[in] param local model
     * @return std::pair<size_t, size_t> [sent, recv] message size
     *
     * If we are connected to the pserver, the local model will be merged remotely with the current global model on the server, and the new global model returned. The internal global model will be replaced by this.
     * If we are *not* connected to the pserver, the local model will be merged into the internal global model
     */
    virtual std::pair<size_t, size_t> sync_param(ParamInterface const* param);


    /**
     * @brief Set the statistic used for the anomaly detection
     */
    void setStatistic(OutlierStatistic to){ m_statistic = to; }

    /**
     * @brief Extract the appropriate statistic from an ExecData_t object
     */
    double getStatisticValue(const ExecData_t &e) const;
    
  protected:
    int m_rank;                              /**< this process rank                      */
    bool m_use_ps;                           /**< true if the parameter server is in use */
    ADNetClient* m_net_client;                 /**< interface for communicating to parameter server */

    std::unordered_map< std::array<unsigned long, 4>, size_t, ArrayHasher<unsigned long,4> > m_local_func_exec_count; /**< Map(program id, rank id, thread id, func id) -> number of times encountered on this node*/

    const ExecDataMap_t * m_execDataMap;     /**< execution data map */
    ParamInterface * m_param;                /**< global parameters (kept in sync with parameter server) */

    PerfStats *m_perf;
  private:
    OutlierStatistic m_statistic; /** Which statistic to use for outlier detection */
    std::unordered_set<std::string> m_func_ignore; /**< A list of functions that are ignored by the anomaly detection (all flagged as normal events)*/
  };

  /**
   * @brief statistic analysis based anomaly detection algorithm
   *
   */
  class ADOutlierSSTD : public ADOutlier {

  public:
    /**
     * @brief Construct a new ADOutlierSSTD object
     *
     */
    ADOutlierSSTD(OutlierStatistic stat = ExclusiveRuntime, double sigma = 6.0);
    /**
     * @brief Destroy the ADOutlierSSTD object
     *
     */
    ~ADOutlierSSTD();

    /**
     * @brief Set the sigma value
     *
     * @param sigma sigma value
     */
    void set_sigma(double sigma) { m_sigma = sigma; }

    /**
     * @brief run this anomaly detection algorithm
     *
     * @param step step (or frame) number
     * @return data structure containing captured anomalies
     */
    Anomalies run(int step=0) override;

  protected:
    /**
     * @brief compute outliers (or anomalies) of the list of function calls
     *
     * @param[out] outliers Array of function calls that were tagged as outliers
     * @param func_id function id
     * @param data[in,out] a list of function calls to inspect
     * @return unsigned long the number of outliers (or anomalies)
     */
    unsigned long compute_outliers(Anomalies &outliers,
				   const unsigned long func_id, std::vector<CallListIterator_t>& data) override;

    /**
     * @brief Compute the anomaly score (probability) for an event assuming a Gaussian distribution
     */
    double computeScore(CallListIterator_t ev, const SstdParam &stats) const;

  private:
    double m_sigma; /**< sigma */
  };

  /**
   * @brief HBOS anomaly detection algorithm
   *
   */
  class ADOutlierHBOS : public ADOutlier {
  public:

    /**
     * @brief Construct a new ADOutlierHBOS object
     * @param stat Which statistic to use
     * @param threshold The threshold defining an outlier
     * @param use_global_threshold The threshold is maintained as part of the global model
     * @param maxbins The maximum number of bins in the histograms
     *
     */
    ADOutlierHBOS(OutlierStatistic stat = ExclusiveRuntime, double threshold = 0.99, bool use_global_threshold = true, int maxbins = 200);

    /**
     * @brief Destroy the ADOutlierHBOS object
     *
     */
    ~ADOutlierHBOS();

    /**
     * @brief Set the outlier detection threshold
     */
    void set_threshold(double to){ m_threshold = to; }

    /**
     * @brief Set the alpha value
     *
     * @param regularizer alpha value
     */
    void set_alpha(double alpha) { m_alpha = alpha; }

    /**
     * @brief Set the maximum number of histogram bins
     */
    void set_maxbins(int to){ m_maxbins = to; }
    
    /**
     * @brief run HBOS anomaly detection algorithm
     *
     * @param step step (or frame) number
     * @return data structure containing captured anomalies
     */
    Anomalies run(int step=0) override;

    /**
     * @brief Override the default threshold for a particular function
     * @param func The function name
     * @param threshold The new threshold
     *
     * Note: during the merge on the pserver, the merged threshold will be the larger of the values from the two inputs, hence the threshold should ideally be uniformly set for all ranks     
     */
    void overrideFuncThreshold(const std::string &func, const double threshold){ m_func_threshold_override[func] = threshold; }

    /**
     * @brief Get the threshold used by the given function
     */
    double getFunctionThreshold(const std::string &fname) const;

  protected:
    /**
     * @brief compute outliers (or anomalies) of the list of function calls
     *
     * @param[out] outliers Array of function calls that were tagged as outliers
     * @param func_id function id
     * @param data[in,out] a list of function calls to inspect
     * @return unsigned long the number of outliers (or anomalies)
     */
    unsigned long compute_outliers(Anomalies &outliers,
				   const unsigned long func_id, std::vector<CallListIterator_t>& data) override;

    /**
     * @brief Scott's rule for bin_width estimation during histogram formation
     */
    double _scott_binWidth(std::vector<double>& vals);
    
  private:
    double m_alpha; /**< Used to prevent log2 overflow */
    double m_threshold; /**< Threshold used to filter anomalies in HBOS*/
    std::unordered_map<std::string, double> m_func_threshold_override; /**< Optionally override the threshold for specific functions*/

    bool m_use_global_threshold; /**< Flag to use global threshold*/

    OutlierStatistic m_statistic; /**< Which statistic to use for outlier detection */
    int m_maxbins; /**< Maximum number of bin in the histograms */
  };

  /**
   * @brief COPOD anomaly detection algorithm
   *
   * A description of the algorithm can be found in 
   * Li, Z., Zhao, Y., Botta, N., Ionescu, C. and Hu, X. COPOD: Copula-Based Outlier Detection. IEEE International Conference on Data Mining (ICDM), 2020.
   * https://www.andrew.cmu.edu/user/yuezhao2/papers/20-icdm-copod-preprint.pdf
   *
   * The implementation is based on the pyOD implementation https://pyod.readthedocs.io/en/latest/_modules/pyod/models/copod.html
   * for which the computation of the outlier score differs slightly from that in the paper
   */
  class ADOutlierCOPOD : public ADOutlier {
  public:

    /**
     * @brief Construct a new ADOutlierCOPOD object
     *
     */
    ADOutlierCOPOD(OutlierStatistic stat = ExclusiveRuntime, double threshold = 0.99, bool use_global_threshold = true);

    /**
     * @brief Destroy the ADOutlierCOPOD object
     *
     */
    ~ADOutlierCOPOD();

    /**
     * @brief Set the alpha value
     *
     * @param regularizer alpha value
     */
    void set_alpha(double alpha) { m_alpha = alpha; }

    /**
     * @brief run HBOS anomaly detection algorithm
     *
     * @param step step (or frame) number
     * @return data structure containing captured anomalies
     */
    Anomalies run(int step=0) override;

    /**
     * @brief Override the default threshold for a particular function
     * @param func The function name
     * @param threshold The new threshold
     *
     * Note: during the merge on the pserver, the merged threshold will be the larger of the values from the two inputs, hence the threshold should ideally be uniformly set for all ranks     
     */
    void overrideFuncThreshold(const std::string &func, const double threshold){ m_func_threshold_override[func] = threshold; }

    /**
     * @brief Get the threshold used by the given function
     */
    double getFunctionThreshold(const std::string &fname) const;


  protected:
    /**
     * @brief compute outliers (or anomalies) of the list of function calls
     *
     * @param[out] outliers Array of function calls that were tagged as outliers
     * @param func_id function id
     * @param data[in,out] a list of function calls to inspect
     * @return unsigned long the number of outliers (or anomalies)
     */
    unsigned long compute_outliers(Anomalies &outliers,
				   const unsigned long func_id, std::vector<CallListIterator_t>& data) override;

    /**
     * @brief Scott's rule for bin_width estimation during histogram formation
     */
    double _scott_binWidth(std::vector<double>& vals);

    /**
     * @brief Computes Empirical CDF of input vector of function runtimes
     */
    std::vector<double> empiricalCDF(const std::vector<double>& runtimes, const bool sorted=true);

  private:
    double m_alpha; /**< Used to prevent log2 overflow */
    double m_threshold; /**< Threshold used to filter anomalies in COPOD*/
    std::unordered_map<std::string, double> m_func_threshold_override; /**< Optionally override the threshold for specific functions*/
    
    bool m_use_global_threshold; /**< Flag to use global threshold*/
    //double m_threshold; /**< sync with global threshold */
    OutlierStatistic m_statistic; /**< Which statistic to use for outlier detection */
  };


} // end of AD namespace
