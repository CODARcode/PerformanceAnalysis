#pragma once
#include<array>
#include "chimbuko/chimbuko.hpp"
#include "chimbuko/ad/ADEvent.hpp"
#include "chimbuko/ad/ExecData.hpp"
#include "chimbuko/util/RunStats.hpp"
#include "chimbuko/param.hpp"
#include "chimbuko/util/hash.hpp"
#include "chimbuko/ad/ADNetClient.hpp"
#include "chimbuko/util/PerfStats.hpp"
#include "chimbuko/util/Anomalies.hpp"

namespace chimbuko {
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
     * @brief Construct a new ADOutlier object
     *
     */
    ADOutlier(OutlierStatistic stat = ExclusiveRuntime);
    /**
     * @brief Destroy the ADOutlier object
     *
     */
    virtual ~ADOutlier();


    static ADOutlier *set_algorithm(OutlierStatistic stat, const ChimbukoParams & params); //const std::string & algorithm);

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
     * @brief abstract method to update local parameters and get global ones
     *
     * @param[in] param local parameters
     * @return std::pair<size_t, size_t> [sent, recv] message size
     */
    virtual std::pair<size_t, size_t> sync_param(ParamInterface const* param) = 0;


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
     * @brief Send the local statistics to the parameter server and update the stored global parameters with the resulting updated statistics
     * @param param The local statistics
     */
    std::pair<size_t, size_t> sync_param(ParamInterface const* param) override;

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
     *
     */
    ADOutlierHBOS(OutlierStatistic stat = ExclusiveRuntime, double threshold = 0.99);
    /**
     * @brief Destroy the ADOutlierHBOS object
     *
     */
    ~ADOutlierHBOS();

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
     * @brief Send the local statistics to the parameter server and update the stored global parameters with the resulting updated statistics
     * @param param The local statistics
     */
    std::pair<size_t, size_t> sync_param(ParamInterface const* param) override;

    /**
     * scott's rule for bin_width estimation
     */
    double _scott_binWidth(std::vector<double>& vals);

    int np_digitize_get_bin_inds(const double& X, const std::vector<double>& bin_edges);

  private:
    double m_alpha; /**< alpha */
    double m_threshold; /**< Threshold used to filter anomalies in HBOS*/
    //double m_threshold; /** sync with global threshold */
    OutlierStatistic m_statistic; /** Which statistic to use for outlier detection */

  };


} // end of AD namespace
