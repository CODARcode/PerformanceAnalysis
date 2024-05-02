#pragma once
#include <chimbuko_config.h>
#include "chimbuko/AD.hpp"
#include "chimbuko/core/util/pointerRegistry.hpp"

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
    std::string ad_algorithm; /**< Algorithm for Anomaly Detection. Set in config file*/

    double outlier_sigma; /**< The number of sigma (standard deviations) away from the mean runtime for an event to be considered anomalous */

    double hbos_threshold; /**< Threshold used by HBOS algorithm to filter outliers. Set in config file*/
    bool hbos_use_global_threshold; /**< Global threshold flag in HBOS*/
    int hbos_max_bins; /**< Maximum number of bins to use in HBOS algorithm histograms*/

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
   * @brief Parameters for setting up the AD
   */
  struct ChimbukoParams{
    //Parameters associated with obtaining trace data from the instrumented binary
    std::string trace_engineType; /**< The ADIOS2 communications mode. If "SST" it will receive trace data in real-time, if "BPfile" it will parse an existing trace dump*/
    std::string trace_data_dir; /**< Directory containing input file.*/
    std::string trace_inputFile; /**< The input file. Assuming the environment variable TAU_FILENAME is set, the binary name is BINARY_NAME and the MPI rank is WORLD_RANK, the file format is
			      < inputFile = "${TAU_FILENAME}-${BINARY_NAME}-${WORLD_RANK}.bp"
			      < Do not include the .sst file extensions for SST mode*/
    int trace_connect_timeout; /**< Timeout (in seconds) of ADIOS2 SST connection to trace data*/

    bool override_rank; /**<Set Chimbuko to overwrite the rank index in the parsed data with its own rank parameter. This disables verification of the data rank.*/

    std::string func_threshold_file; /**< A filename containing HBOS/COPOD algorithm threshold overrides for specified functions. Format is JSON: "[ { "fname": <FUNC>, "threshold": <THRES> },... ]". Empty string (default) uses default threshold for all funcs*/ 

    std::string ignored_func_file; /**< A filename containing function names (one per line) which the AD algorithm will ignore. All such events are labeled as normal. Empty string (default) performs AD on all events*/

    std::string monitoring_watchlist_file; /**< A filename containing the counter watchlist for the integration with the monitoring plugin. Empty string uses the default subset.   */
    std::string monitoring_counter_prefix; /**< An optional prefix marking a set of monitoring plugin counters to be captured, on top of or superseding the watchlist. Empty string (default) is ignored.*/


    unsigned long prov_min_anom_time; /**< The minimum exclusive runtime (in microseconds) for anomalies recorded in the provenance output (default 0) */

    unsigned int anom_win_size; /**< When anomaly data are recorded, a window of this size (in units of events) around the anomalous event are also recorded (used both for viz and provDB)*/

    int parser_beginstep_timeout; /**< Set the timeout in seconds on waiting for the next ADIOS2 timestep (default 30s)*/

    std::string outlier_statistic; /**< Set the statistic used in outlier detection*/

    std::string read_ignored_corrid_funcs; /**< The path to a file containing functions for which the correlation ID counter should be ignored. If an empty string (default) no IDs will be ignored*/

    ChimbukoBaseParams base_params; /**< General parameters for the Chimbuko system */

    ChimbukoParams();

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


  /**
   * @brief Record the state of accumulated statistics
   */
  struct AccumValuesPrd{
    unsigned long long n_func_events; /**< Total number of function events since last write of periodic data*/
    unsigned long long n_comm_events; /**< Total number of comm events since last write of periodic data*/
    unsigned long long n_counter_events; /**< Total number of counter events since last write of periodic data*/

    void reset(){
      n_func_events = n_comm_events = n_counter_events = 0;
    }     

    AccumValuesPrd(){ reset(); }
  };

  struct RunStatistics{
    unsigned long long n_func_events; /**< Total number of function events processed in the run*/
    unsigned long long n_comm_events; /**< Total number of comm events processed in the run*/
    unsigned long long n_counter_events; /**< Total number of counter events processed in the run*/

    void reset(){
      n_func_events = n_comm_events = n_counter_events = 0;
    }     

    RunStatistics(){ reset(); }
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
    void initializeBase(const ChimbukoBaseParams &params);
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
    ChimbukoBase(const ChimbukoBaseParams &params): ChimbukoBase(){ initializeBase(params); }
    ~ChimbukoBase();
    
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
    void init_provdb();
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

  /**
   * @brief The main interface for the AD module
   */
  class Chimbuko: public ChimbukoBase{
  private:
  public:
    Chimbuko();
    ~Chimbuko();

    /**
     * @brief Construct and initialize the AD with the parameters provided
     */
    Chimbuko(const ChimbukoParams &params): Chimbuko(){ initialize(params); }

    /**
     * @brief Initialize the AD with the parameters provided (must be performed prior to running)
     */
    void initialize(const ChimbukoParams &params);


    /**
     * @brief Free memory associated with AD components (called automatically by destructor)
     */
    void finalize();

    /**
     * @brief Request that the event manager print its status
     */
    void show_status(bool verbose=false ) const { m_event->show_status(verbose); }

    /**
     * @brief Whether the AD is connected through ADIOS2 to the trace input
     */
    bool get_status() const { return m_parser->getStatus(); }

    /**
     * @brief Get the current IO step
     */
    //int get_step() const { return m_parser->getCurrentStep(); }   

    /**
     * @brief Read a step's worth of data, returning an interface to the data and the step index
     * @return true if the read was unsuccessful or there are no more steps to read
     * @param[out] iface An interface instance to access the data and labeling
     */
    bool readStep(std::unique_ptr<ADDataInterface> &iface) override;

    /**
     * @brief Record accumulated performance data since the last call to this function, and reset
     */
    void recordResetPeriodicPerfData(PerfPeriodic &perf_prd) override;

    /**
     * @brief This function is called at the beginning to reset the run statistics if set by a previous call to "run"
     */
    void resetRunStatistics() override{ m_run_stats.reset(); }

    /**
     * @brief Purge data that has been labeled, recorded and is no longer needed 
     */
    void performEndStepAction() override;

    /**
     * @brief Get the run stats stored by the main class
     */
    RunStatistics const & getRunStats() const{ return m_run_stats; }


  private:
    //Initialize various components; called by initialize method
    void init_parser();
    void init_event();
    void init_counter();

    void init_provenance_gatherer();
    void init_metadata_parser();
    void init_monitoring();

    /**
     * @brief Signal the parser to parse the adios2 timestep
     * @param[out] number of func events parsed
     * @param[out] number of comm events parsed
     * @param[out] number of counter events parsed
     * @param[in] expect_step The expected step index (cross-checked against value in data stream)
     * @return false if unsuccessful, true otherwise
     */
    bool parseInputStep(unsigned long long& n_func_events,
			unsigned long long& n_comm_events,
			unsigned long long& n_counter_event,
			const int expect_step);

    /**
     * @brief Extract parsed events and insert into the event manager
     * @param[out] first_event_ts Earliest timestamp in io frame
     * @param[out] last_event_ts Latest timestamp in io frame
     * @param step The adios2 stream step index
     */
    void extractEvents(unsigned long &first_event_ts,
		       unsigned long &last_event_ts,
		       int step);

    /**
     * @brief Extract parsed counters and insert into counter manager
     * @param rank The MPI rank of the process
     * @param step The adios2 stream step index
     */
    void extractCounters(int rank, int step);

    /**
     * @brief Update the node state using information from TAU's monitoring plugin
     */
    void extractNodeState();


    /**
     * @brief Extract provenance information about anomalies and store in a buffer  (implementation defined) -- implementation of buffering, called by extractProvenance
     * @param[out] buffer The buffer; a map between a collection name and an array of entries
     * @param[in] anomalies The interface to the output of the AD
     * @param[in] step The step index
     */
    void bufferStoreProvenanceDataImplementation(std::unordered_map<std::string, std::vector<nlohmann::json> > &buffer, const ADDataInterface &anomalies, const int step) override;

    /**
     * @brief Gather and buffer the required statistics data for the pserver
     */
    void bufferStorePSdata(const ADDataInterface &anomalies,  const int step) override;


    /**
     * @brief Implementation of data send (buffer format is implementation defined)
     */
    void sendPSdataImplementation(ADNetClient &net_client, const int step) override;    

    //Components and parameters
    ADParser * m_parser;       /**< adios2 input data stream parser */
    ADEvent * m_event;         /**< func/comm event manager */
    ADCounter * m_counter;     /**< counter event manager */
    ADMetadataParser *m_metadata_parser; /**< parser for metadata */
    ADAnomalyProvenance  *m_anomaly_provenance; /**< provenance information gatherer*/ 

    ADMonitoring *m_monitoring; /**< maintain the node state by parsing counters from TAU's monitoring plugin*/

    PointerRegistry m_ptr_registry; /**< Managers pointers to components ensuring they are erased in the correct order*/

    ChimbukoParams m_params; /**< Parameters to setup the AD*/
    bool m_is_initialized; /**< Whether the AD has been initialized*/

    int m_program_idx; /**< The program index; alias of base analysis objective index*/

    //State of currently stored function executions
    unsigned long m_execdata_first_event_ts; /**< earliest timestamp in current execution data */
    unsigned long m_execdata_last_event_ts; /**< latest timestamp in current execution data */
    bool m_execdata_first_event_ts_set; /**< False if the first event ts has not yet been set*/

    //State of accumulated statistics
    AccumValuesPrd m_accum_prd;
    RunStatistics m_run_stats; /**< Statistics for current/last call to "run"*/

    std::set<unsigned long> m_exec_ignore_counters; /**< Counter indices in this list are ignored by the event manager (but will still be picked up by other components)*/

    ADExecDataInterface::FunctionsSeenType m_func_seen; /**< If using SST algorithm, we record whether a function has previously been seen in order to avoid including the first call to a function in the data*/

    std::vector<ADLocalFuncStatistics> m_funcstats_buf; /**< Buffered function statistics data waiting to be sent*/
    std::vector<ADLocalCounterStatistics> m_countstats_buf;  /**< Buffered counter statistics data waiting to be sent*/
    std::vector<ADLocalAnomalyMetrics> m_anom_metrics_buf;  /**< Buffered anomaly metrics data waiting to be sent*/    

    ADEvent::purgeReport m_purge_report; /**< Report information from last event purge*/
  };

}
