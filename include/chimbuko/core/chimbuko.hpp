#pragma once
#include <chimbuko_config.h>
#include <chimbuko/core/util/pointerRegistry.hpp>
#include <chimbuko/core/ad/ADNetClient.hpp>
#include <chimbuko/core/util/PerfStats.hpp>
#include <chimbuko/core/ad/ADOutlier.hpp>
#include <chimbuko/core/ad/ADProvenanceDBclient.hpp>
#include <chimbuko/core/ad/ADio.hpp>
#include <chimbuko/core/provdb/ProvDBmoduleSetupCore.hpp>

namespace chimbuko {

  struct ChimbukoBaseParams{

    std::string prov_outputpath; /**< Directory where provenance data is written (in conjunction with provDB if active). Blank string indicates no output*/
#ifdef ENABLE_PROVDB
    //Parameters associated with the provenance database
    std::string provdb_addr_dir; /**< Directory in which the provenance database writes its address files. If an empty string the provDB will not be used*/
    int nprovdb_shards; /**< Number of database shards*/
    int nprovdb_instances; /**< Number of instances of the provenance database server*/
    std::string provdb_mercury_auth_key; /**< An authorization key for initializing Mercury (optional, default "")*/
#endif
    int prov_record_startstep; /**< If != -1, the IO step on which to start recording provenance information for anomalies */
    int prov_record_stopstep; /**< If != -1, the IO step on which to stop recording provenance information for anomalies */
    int prov_io_freq; /**< The frequency, in steps, at which provenance data is written/sent to the provDB. For steps between it is buffered.*/

    //Parameters associated with the outlier detection algorithm
    ADOutlier::AlgoParams algo_params; /**< The algorithm parameters */
    
    int global_model_sync_freq; /**< How often (in steps) the global model is updated (default 1)*/

    //Parameters associated with communicating with the parameter server
    std::string pserver_addr; /**< The address of the parameter server.
				 < If no parameter server is in use, this string should be empty (length zero)
				 < If using ZmqNet (default) this is a tcp address of the form "tcp://${ADDRESS}:${PORT}"
			      */
    int ps_send_stats_freq; /**< How often in steps the statistics data is uploaded to the pserver (default 1) */

    int hpserver_nthr;        /**< If using the hierarchical pserver, this parameter is used to compute a port offset for the particular endpoint that this AD rank connects to */

    //Parameters associated with performance analysis of AD module
    std::string perf_outputpath; /**< Output path for AD performance monitoring data. If an empty string no output is written.*/
    int perf_step; /**<How frequently (in IO steps) the performance data is dumped*/

    //General parameters for Chimbuko
    bool verbose; /**< Enable verbose output. Typically one enables this only on a single node (eg verbose = (rank==0); ) */
    bool only_one_frame; /**< Force the AD to stop after a single IO frame */
    int interval_msec; /**< Force the AD to pause for this number of ms at the end of each IO step*/
    int max_frames; /**< Stop analyzing data once this number of IO frames have been read. A value < 0 (default) is unlimited*/

    std::string err_outputpath; /**< Output path for error logs. If empty errors will be sent to std::cerr*/

    int step_report_freq; /**<Steps between Chimbuko reporting IO step progress. Use 0 to deactivate this logging entirely (default 1)*/
    
    int net_recv_timeout; /**< Timeout (in ms) used for blocking receives functionality on client (driver) of parameter server */

    int analysis_step_freq; /**< The frequency in IO steps at which we perform the anomaly detection. Events are retained between io steps if the analysis is not run. (default 1)*/

    int ana_obj_idx; /**< Analysis objective index (use with the rank to differentiate instances)*/
    int rank; /**< The rank index of the this instance*/

    ChimbukoBaseParams();

    void print() const;
  };


  /**
   * @brief Record the state of accumulated statistics
   */
  struct AccumValuesPrdBase{
    unsigned long n_outliers; /**< Total number of outiers detected since last write of periodic data*/
    unsigned long pserver_sync_time_ms; /**< Time in ms spent synchronizing with the pserver since the last reset*/
    int n_steps; /**< Number of steps since last write of periodic data */

    void reset(){
      n_outliers = pserver_sync_time_ms = 0;
      n_steps = 0;
    }     

    AccumValuesPrdBase(){ reset(); }
  };

  struct RunStatisticsBase{
    unsigned long long n_outliers; /**< Total number of outliers recorded in the run*/

    void reset(){
      n_outliers = 0;
    }     

    RunStatisticsBase(){ reset(); }
  };



  class ChimbukoBase{
  protected:
    /**
     * @brief Initialize the base class
     * @param params The optional and mandatory parameters of the instance
     * @param pdb_setup An object specify the list of collections for the provDB
     */
    void initializeBase(const ChimbukoBaseParams &params
#ifdef ENABLE_PROVDB
			, const ProvDBmoduleSetupCore &pdb_setup
#endif
			);
    void finalizeBase();
    /**
     * @brief Query whether to produce verbose report output for a given step
     */
    bool doStepReport(int step) const;
   
    /**
     * @brief Query whether the AD analysis will be performed on a given step
     */
    bool doAnalysisOnStep(int step) const;

    /**
     * @brief Access the aggregate performance metric recorder
     */
    PerfStats &getPerfRecorder(){ return m_perf; }

    /**
     * @brief Access the periodic performance metric recorder
     */
    PerfPeriodic &getPerfPrdRecorder(){ return m_perf_prd; }

    /**
     * @brief Access the network interface
     */
    ADNetClient & getNetClient() const;
       
  public:
    ChimbukoBase();    
    ChimbukoBase(const ChimbukoBaseParams &params
#ifdef ENABLE_PROVDB
		 , const ProvDBmoduleSetupCore &pdb_setup
#endif
		 );
    virtual ~ChimbukoBase();
    
    /**
     * @brief Read a step's worth of data, returning an interface to the data and the step index
     * @return true if the read was unsuccessful or there are no more steps to read
     * @param[out] iface An interface instance to access the data and labeling
     *
     * Note, the iface pointer can be left as null if doAnalysisOnStep(step) == false
     */
    virtual bool readStep(std::unique_ptr<ADDataInterface> &iface) = 0;

    /**
     * @brief Extract provenance information about anomalies and store in a buffer
     */
    void bufferStoreProvenanceData(const ADDataInterface &anomalies);
    
    /**
     * @brief Extract provenance information about anomalies and store in the provided buffer: called by bufferStoreProvenanceData
     * @param[out] buffer The buffer; a map between a collection name and an array of entries
     * @param[in] anomalies The interface to the output of the AD
     * @param[in] step The step index
     */
    virtual void bufferStoreProvenanceDataImplementation(std::unordered_map<std::string, std::vector<nlohmann::json> > &buffer, const ADDataInterface &anomalies, const int step) = 0;
    
    /**
     * @brief If step % m_params.prov_io_freq == 0  OR  force == true , write or communicate provenance data buffers to provenance DB, then flush buffers
     */
    void sendProvenance(bool force = false);

    /**
     * @brief Gather and buffer the required statistics data for the pserver
     */
    virtual void bufferStorePSdata(const ADDataInterface &anomalies,  const int step) = 0;

    /**
     * @brief Send buffered pserver statistics data when step matches frequency or force==true
     */
    void sendPSdata(bool force = false);

    /**
     * @brief Implementation of data send (buffer format is implementation defined)
     */
    virtual void sendPSdataImplementation(ADNetClient &net_client, const int step) = 0;

    /**
     * @brief (Optional) Record accumulated performance data since the last call to this function, and reset
     */
    virtual void recordResetPeriodicPerfData(PerfPeriodic &perf_prd){};

    /**
     * @brief (Optional) Perform some action at the end of the step, after the analysis and recording of information
     *
     * Example: purging data that has been labeled, recorded and is no longer needed
     */
    virtual void performEndStepAction(){};

    /**
     * @brief (Optional) If the derived class collects statistics for a call to "run", this function is called at the beginning to reset those statistics if set by a previous call to "run"
     */
    virtual void resetRunStatistics(){}

#ifdef ENABLE_PROVDB
    /**
     * @brief Whether the provenance database is in use
     */
    bool use_provdb() const{ return m_provdb_client->isConnected(); }

    /**
     * @brief Return the provenance DB client
     */
    ADProvenanceDBclient & getProvenanceDBclient(){ return *m_provdb_client; }
#endif

    /**
     * @brief Whether the parameter server is in use
     */
    bool usePS() const{ return m_net_client != nullptr; }

    /**
     * @brief Return the rank of this instance
     */
    int getRank() const{ return m_base_params.rank; }

    /**
     * @brief Get the analysis objective index
     */
    int getAnalysisObjectiveIdx() const{ return m_base_params.ana_obj_idx; }

    /**
     * @brief Run a single frame (step) of the analysis
     * @return True if a step was analyzed. A false value is used to indicate the end of the run
     */
    bool runFrame();
    /**
     * @brief Run the analysis
     * @return The number of steps that were analyzed in this call to run 
     *
     * This is not the same as the step index if run is called multiple times with early termination
     */
    int run();

    /**
     * @brief Get the current step index
     */
    int getStep() const{ return m_step; }
    
    /**
     * @brief (Const) access to the AD algorithm instance
     */
    ADOutlier const & getAD() const{ return *m_outlier; }

    /**
     * @brief (Const) access to the base parameters
     */    
    ChimbukoBaseParams const & getBaseParams() const{ return m_base_params; }

    /**
     * @brief Get the run stats stored by the base class
     */
    RunStatisticsBase const & getBaseRunStats() const{ return m_run_stats; }
    
  private:
    void init_io();
    void init_outlier();
#ifdef ENABLE_PROVDB
    void init_provdb(const ProvDBmoduleSetupCore &pdb_setup);
#endif
    void init_net_client();

    bool m_base_is_initialized;

    ChimbukoBaseParams m_base_params;
#ifdef ENABLE_PROVDB
    ADProvenanceDBclient *m_provdb_client; /**< provenance DB client*/
#endif
    ADOutlier * m_outlier;     /**< outlier detection algorithm */
    ADio * m_io;               /**< output writer */
    ADThreadNetClient * m_net_client; /**< client for comms with parameter server */

    PointerRegistry m_ptr_registry; /**< Managers pointers to components ensuring they are erased in the correct order*/

    //State of accumulated statistics
    AccumValuesPrdBase m_accum_prd;
    RunStatisticsBase m_run_stats;

    int m_step; /**< The io step / frame index*/

    std::unordered_map<std::string, std::vector<nlohmann::json> > m_provdata_buf; /**< Buffering of provenance data, flushed on send */

    mutable PerfStats m_perf; /**< Performance data */
    mutable PerfPeriodic m_perf_prd; /**<Performance temporal logging */
  };

}
