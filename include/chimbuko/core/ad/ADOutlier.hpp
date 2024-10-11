#pragma once
#include <chimbuko_config.h>
#include <array>
#include <unordered_set>
#include <nlohmann/json.hpp>

#include "chimbuko/core/util/RunStats.hpp"
#include "chimbuko/core/param.hpp"
#include "chimbuko/core/param/sstd_param.hpp"
#include "chimbuko/core/util/hash.hpp"
#include "chimbuko/core/ad/ADNetClient.hpp"
#include "chimbuko/core/util/PerfStats.hpp"
#include "chimbuko/core/ad/ADDataInterface.hpp"
#include "chimbuko/core/util/commandLineParser.hpp"

namespace chimbuko { 
  /**
   * @brief abstract class for anomaly detection algorithms
   *
   */
  class ADOutlier {
  public:
    /**
     * @brief Unified structure for passing the parameters of the AD algorithms to the factory method
     */
    struct AlgoParams{
      std::string algorithm; /**< The string name of the algorithm: "sstd", "hbos", "copod" */
      
      //SSTD
      double sstd_sigma; /**< The number of sigma that defines an outlier*/
    
      //HBOS/COPOD
      double hbos_thres; /**< The outlier threshold*/
      //std::string func_threshold_file; /**< Name of a file containing per-function overrides of the threshold*/

      //HBOS
      bool glob_thres; /**< Should the global threshold be used? */
      int hbos_max_bins; /**< The maximum number of bins in a histogram */

      AlgoParams();

      /**
       * @brief Read the parameters from a json object. Note, only "algorithm" and the entries associated with the specific algorithm need to be set
       */
      void setJson(const nlohmann::json &in);

      /**
       * @brief Read the parameters from a json file. Note, only "algorithm" and the entries associated with the specific algorithm need to be set
       */     
      void loadJsonFile(const std::string &filename);

      /**
       * @brief Return the parameters as a json object
       */
      nlohmann::json getJson() const;

      /**
       * @brief Equivalence operator
       */
      bool operator==(const AlgoParams &r) const;

      /**
       * @brief Parser object that reads the data from a json file with provided filename
       */
      class cmdlineParser: public optionalCommandLineArgBase{
      private:
	std::string m_arg; /**< The argument, format "-a" */
	std::string m_help_str; /**< The help string */
	AlgoParams &member;
      public:
	cmdlineParser(AlgoParams &member, const std::string &arg, const std::string &help_str): m_arg(arg), m_help_str(help_str), member(member){}

	/**
	 * @brief If the first string matches the internal arg string (eg "-help"), a number of strings are consumed from the array 'vals' and that number returned. 
	 * A value of -1 indicates the argument did not match.
	 *
	 * @param vals An array of strings
	 * @param vals_size The length of the string array
	 */
	int parse(const std::string &arg, const char** vals, const int vals_size) override;
	/**
	 * @brief Print the help string for this argument to the ostream
	 */
	void help(std::ostream &os) const override;
      };
    };


    /**
     * @brief Construct a new ADOutlier object
     * @param rank The rank of this instance
     */
    ADOutlier(int rank);
    /**
     * @brief Destroy the ADOutlier object
     *
     */
    virtual ~ADOutlier();

    /**
     * @brief Factory method to select AD algorithm at runtime
     */
    static ADOutlier *set_algorithm(int rank, const AlgoParams &params);

    /**
     * @brief check if the parameter server is in use
     *
     * @return true if the parameter server is in use
     * @return false if the parameter server is not in use
     */
    bool use_ps() const { return m_use_ps; }

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
    virtual void run(ADDataInterface &data, int step=0) = 0;

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
     * @brief Set the global parameters, overwriting the existing global model. Here the input is in serialized form
     *
     * Use in conjunction with setGlobalModelSyncFrequency(0) to set and freeze the model, not allowing it to be modified by the data
     */
    void setGlobalParameters(const std::string &to);

    /**
     * @brief Set how often (in steps, or equivalently calls to "run") the global model is updated. If  to <= 0, the global model will never be updated
     */
    void setGlobalModelSyncFrequency(int to){ m_global_model_sync_freq = to; }

    /**
     * @brief Return the algorithm name
     */
    virtual std::string getAlgorithmName() const = 0;
  protected:
    /** @brief Synchronize the input model with the global model    
     *
     * @param param the input model
     * @return std::pair<size_t, size_t> [sent, recv] message size
     *
     * If we are connected to the pserver, the local model will be merged remotely with the current global model on the server, and the new global model returned. The internal global model will be replaced by this.
     * If we are *not* connected to the pserver, the local model will be merged into the internal global model
     */
    virtual std::pair<size_t, size_t> sync_param(ParamInterface *param);

    /**
     * @brief Every m_global_model_sync_freq calls to this function, synchronize the local model with the global model then flush the local model 
     */
    void updateGlobalModel();

  protected:
    bool m_use_ps;                           /**< true if the parameter server is in use */
    ADNetClient* m_net_client;                 /**< interface for communicating to parameter server */

    ParamInterface * m_param;                /**< global parameters (kept in sync with parameter server) */
    ParamInterface * m_local_param;          /**< local parameters that have not yet been sync'd with the global model */
    int m_sync_call_count;                   /**< count of calls to sync_param */
    int m_global_model_sync_freq;            /**< how often the local model is pushed and synchronized with the globel model (default 1)*/

    int m_rank;                              /**< rank index, used only for staggering pserver sync events*/
    PerfStats *m_perf;
  };

  /**
   * @brief statistic analysis based anomaly detection algorithm
   *
   */
  class ADOutlierSSTD : public ADOutlier{

  public:
    /**
     * @brief Construct a new ADOutlierSSTD object
     *
     */
    ADOutlierSSTD(int rank, double sigma = 6.0);
    /**
     * @brief Destroy the ADOutlierSSTD object
     *
     */
    ~ADOutlierSSTD();

    /**
     * @brief Set the sigma value: the number of standard deviations from the mean that defines an anomaly
     *
     * @param sigma sigma value
     */
    void set_sigma(double sigma) { m_sigma = sigma; }

    void run(ADDataInterface &data, int step=0) override;

    /**
     * @brief Return the algorithm name
     */
    std::string getAlgorithmName() const override{ return "sstd"; }

  protected:

    /**
     * Run the anomaly detection algorithm on the array of data_vals associated with the provided data set index and apply the label through the interface
     */
    void labelData(std::vector<ADDataInterface::Elem> &data_vals, size_t dset_idx, size_t model_idx);

    /**
     * @brief Compute the anomaly score (probability) for an event assuming a Gaussian distribution
     */
    double computeScore(double value, size_t model_idx, const SstdParam &stats) const;

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
     * @param threshold The threshold defining an outlier
     * @param use_global_threshold The threshold is maintained as part of the global model
     * @param maxbins The maximum number of bins in the histograms
     *
     */
    ADOutlierHBOS(int rank, double threshold = 0.99, bool use_global_threshold = true, int maxbins = 200);

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

 
    void run(ADDataInterface &data, int step=0) override;

    /**
     * @brief Return the algorithm name
     */
    std::string getAlgorithmName() const override{ return "hbos"; }

    /**
     * @brief Override the default threshold for a particular function
     * @param func The function name
     * @param threshold The new threshold
     *
     * Note: during the merge on the pserver, the merged threshold will be the larger of the values from the two inputs, hence the threshold should ideally be uniformly set for all ranks     
     */
    //void overrideFuncThreshold(const std::string &func, const double threshold){ m_func_threshold_override[func] = threshold; }

    /**
     * @brief Get the threshold used by the given data set function
     */
    //double getFunctionThreshold(const size_t dset_idx) const;

  protected:
    /**
     * @brief compute outliers (or anomalies) of the list of data
     *
     * @param[in,out] data_vals Array of data
     * @param dset_idx The data set index
     * @param model_idx The index of the data type in the model
     */
    void labelData(std::vector<ADDataInterface::Elem> &data_vals, size_t dset_idx, size_t model_idx);

    /**
     * @brief Scott's rule for bin_width estimation during histogram formation
     */
    double _scott_binWidth(std::vector<double>& vals);
    
  private:
    double m_alpha; /**< Used to prevent log2 overflow */
    double m_threshold; /**< Threshold used to filter anomalies in HBOS*/
    std::unordered_map<std::string, double> m_func_threshold_override; /**< Optionally override the threshold for specific functions*/

    bool m_use_global_threshold; /**< Flag to use global threshold*/

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
    ADOutlierCOPOD(int rank, double threshold = 0.99, bool use_global_threshold = true);

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

    void run(ADDataInterface &data, int step=0) override;

    /**
     * @brief Return the algorithm name
     */
    std::string getAlgorithmName() const override{ return "copod"; }

    /**
     * @brief Override the default threshold for a particular function
     * @param func The function name
     * @param threshold The new threshold
     *
     * Note: during the merge on the pserver, the merged threshold will be the larger of the values from the two inputs, hence the threshold should ideally be uniformly set for all ranks     
     */
    //void overrideFuncThreshold(const std::string &func, const double threshold){ m_func_threshold_override[func] = threshold; }

    /**
     * @brief Get the threshold used by the given function
     */
    //double getFunctionThreshold(const std::string &fname) const;


  protected:

    void labelData(std::vector<ADDataInterface::Elem> &data_vals, size_t dset_idx, size_t model_idx);

  private:
    double m_alpha; /**< Used to prevent log2 overflow */
    double m_threshold; /**< Threshold used to filter anomalies in COPOD*/
    //std::unordered_map<std::string, double> m_func_threshold_override; /**< Optionally override the threshold for specific functions*/
    
    bool m_use_global_threshold; /**< Flag to use global threshold*/
  };


} // end of AD namespace
