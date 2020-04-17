#pragma once
#include<array>
#include "chimbuko/ad/ADEvent.hpp"
#include "chimbuko/ad/ExecData.hpp"
#include "chimbuko/util/RunStats.hpp"
#include "chimbuko/param.hpp"
#include "chimbuko/util/hash.hpp"
#include "chimbuko/ad/ADNetClient.hpp"
#ifdef _PERF_METRIC
#include "chimbuko/util/RunMetric.hpp"
#endif

namespace chimbuko {  
  /**
   * @brief abstract class for anomaly detection algorithms
   * 
   */
  class ADOutlier {

  public:
    /**
     * @brief Construct a new ADOutlier object
     * 
     */
    ADOutlier();
    /**
     * @brief Destroy the ADOutlier object
     * 
     */
    virtual ~ADOutlier();

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
     * @param[out] outliers Array of function calls that were tagged as outliers
     * @param step step (or frame) number
     * @return unsigned long the number of detected outliers
     */
    virtual unsigned long run(std::vector<CallListIterator_t> &outliers, int step=0) = 0;

#ifdef _PERF_METRIC
    void dump_perf(std::string path, std::string filename="metric.json"){
      m_perf.dump(path, filename);
    }
#endif

    /**
     * @brief Get the local copy of the global parameters
     * @return Pointer to a ParamInterface object
     */
    ParamInterface const* get_global_parameters() const{ return m_param; }
  
  protected:
    /**
     * @brief abstract method to compute outliers (or anomalies)
     * 
     * @param[out] outliers Array of function calls that were tagged as outliers
     * @param func_id function id
     * @param[in,out] data a list of function calls to inspect. Entries will be tagged as outliers
     * @param min_ts the minimum timestamp of the list of function calls
     * @param max_ts the maximum timestamp of the list of function calls
     * @return unsigned long the number of outliers (or anomalies)
     */
    virtual unsigned long compute_outliers(std::vector<CallListIterator_t> &outliers,
					   const unsigned long func_id, std::vector<CallListIterator_t>& data,
					   long& min_ts, long& max_ts) = 0;

    /**
     * @brief abstract method to update local parameters and get global ones
     * 
     * @param[in] param local parameters
     * @return std::pair<size_t, size_t> [sent, recv] message size 
     */
    virtual std::pair<size_t, size_t> sync_param(ParamInterface const* param) = 0;
    /**
     * @brief update (send) function statistics (#anomalies, incl/excl run times) gathered during this io step to the connected parameter server
     * 
     * @param l_stats local statistics
     * @param step step (or frame) number
     * @return std::pair<size_t, size_t> [sent, recv] message size
     */
    std::pair<size_t, size_t> update_local_statistics(std::string l_stats, int step);

  protected:
    int m_rank;                              /**< this process rank                      */
    bool m_use_ps;                           /**< true if the parameter server is in use */
    ADNetClient* m_net_client;                 /**< interface for communicating to parameter server */
    
    std::unordered_map< std::array<unsigned long, 4>, size_t, ArrayHasher<unsigned long,4> > m_local_func_exec_count; /**< Map(program id, rank id, thread id, func id) -> number of times encountered on this node*/
    
    const ExecDataMap_t * m_execDataMap;     /**< execution data map */
    ParamInterface * m_param;                /**< global parameters (kept in sync with parameter server) */

#ifdef _PERF_METRIC
    RunMetric m_perf;
#endif
    // number of outliers per function: func id -> # outliers
    // std::unordered_map<unsigned long, unsigned long> m_outliers;
    // inclusive runtime statistics per fucntion: func id -> run stats
    // exclusive runtime statistics per function: func id -> run stats
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
    ADOutlierSSTD();
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
     * @param[out] outliers Array of function calls that were tagged as outliers
     * @param step step (or frame) number
     * @return unsigned long the number of anomalies
     */
    unsigned long run(std::vector<CallListIterator_t> &outliers, int step=0) override;

  protected:
    /**
     * @brief compute outliers (or anomalies) of the list of function calls
     * 
     * @param[out] outliers Array of function calls that were tagged as outliers
     * @param func_id function id
     * @param data[in,out] a list of function calls to inspect
     * @param min_ts the minimum timestamp of the list of function calls
     * @param max_ts the maximum timestamp of the list of function calls
     * @return unsigned long the number of outliers (or anomalies)
     */
    unsigned long compute_outliers(std::vector<CallListIterator_t> &outliers,
				   const unsigned long func_id, std::vector<CallListIterator_t>& data,
				   long& min_ts, long& max_ts) override;


    std::pair<size_t, size_t> sync_param(ParamInterface const* param) override;
    
  private:
    double m_sigma; /**< sigma */
  };

} // end of AD namespace
