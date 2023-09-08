#pragma once
#include <chimbuko_config.h>
#include "chimbuko/AD.hpp"
#include "chimbuko/util/pointerRegistry.hpp"

namespace chimbuko {

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

    std::string ad_algorithm; /**< Algorithm for Anomaly Detection. Set in config file*/
    //Parameters associated with the outlier detection algorithm
    double outlier_sigma; /**< The number of sigma (standard deviations) away from the mean runtime for an event to be considered anomalous */

    double hbos_threshold; /**< Threshold used by HBOS algorithm to filter outliers. Set in config file*/
    bool hbos_use_global_threshold; /**< Global threshold flag in HBOS*/
    int hbos_max_bins; /**< Maximum number of bins to use in HBOS algorithm histograms*/

    std::string func_threshold_file; /**< A filename containing HBOS/COPOD algorithm threshold overrides for specified functions. Format is JSON: "[ { "fname": <FUNC>, "threshold": <THRES> },... ]". Empty string (default) uses default threshold for all funcs*/ 

    std::string ignored_func_file; /**< A filename containing function names (one per line) which the AD algorithm will ignore. All such events are labeled as normal. Empty string (default) performs AD on all events*/

    std::string monitoring_watchlist_file; /**< A filename containing the counter watchlist for the integration with the monitoring plugin. Empty string uses the default subset.   */
    std::string monitoring_counter_prefix; /**< An optional prefix marking a set of monitoring plugin counters to be captured, on top of or superseding the watchlist. Empty string (default) is ignored.*/

    //Parameters associated with communicating with the parameter server
    std::string pserver_addr; /**< The address of the parameter server.
				 < If no parameter server is in use, this string should be empty (length zero)
				 < If using ZmqNet (default) this is a tcp address of the form "tcp://${ADDRESS}:${PORT}"
			      */
    int hpserver_nthr;        /**< If using the hierarchical pserver, this parameter is used to compute a port offset for the particular endpoint that this AD rank connects to */

    std::string prov_outputpath; /**< Directory where provenance data is written (in conjunction with provDB if active). Blank string indicates no output*/
#ifdef ENABLE_PROVDB
    //Parameters associated with the provenance database
    std::string provdb_addr_dir; /**< Directory in which the provenance database writes its address files. If an empty string the provDB will not be used*/
    int nprovdb_shards; /**< Number of database shards*/
    int nprovdb_instances; /**< Number of instances of the provenance database server*/
#endif
    int prov_record_startstep; /**< If != -1, the IO step on which to start recording provenance information for anomalies */
    int prov_record_stopstep; /**< If != -1, the IO step on which to stop recording provenance information for anomalies */
    unsigned long prov_min_anom_time; /**< The minimum exclusive runtime (in microseconds) for anomalies recorded in the provenance output (default 0) */

    unsigned int anom_win_size; /**< When anomaly data are recorded, a window of this size (in units of events) around the anomalous event are also recorded (used both for viz and provDB)*/

    //Parameters associated with performance analysis of AD module
    std::string perf_outputpath; /**< Output path for AD performance monitoring data. If an empty string no output is written.*/
    int perf_step; /**<How frequently (in IO steps) the performance data is dumped*/

    //General parameters for Chimbuko
    int program_idx; /**< Program index (for workflows with >1 component) */
    int rank; /**< The rank index of the trace data*/
    bool verbose; /**< Enable verbose output. Typically one enables this only on a single node (eg verbose = (rank==0); ) */
    bool only_one_frame; /**< Force the AD to stop after a single IO frame */
    int interval_msec; /**< Force the AD to pause for this number of ms at the end of each IO step*/
    int max_frames; /**< Stop analyzing data once this number of IO frames have been read. A value < 0 (default) is unlimited*/

    std::string err_outputpath; /**< Output path for error logs. If empty errors will be sent to std::cerr*/
    int parser_beginstep_timeout; /**< Set the timeout in seconds on waiting for the next ADIOS2 timestep (default 30s)*/

    bool override_rank; /**<Set Chimbuko to overwrite the rank index in the parsed data with its own rank parameter. This disables verification of the data rank.*/

    std::string outlier_statistic; /**< Set the statistic used in outlier detection*/

    int step_report_freq; /**<Steps between Chimbuko reporting IO step progress. Use 0 to deactivate this logging entirely (default 1)*/
    
    int net_recv_timeout; /**< Timeout (in ms) used for blocking receives functionality on client (driver) of parameter server */

    int analysis_step_freq; /**< The frequency in IO steps at which we perform the anomaly detection. Events are retained between io steps if the analysis is not run. (default 1)*/

    std::string read_ignored_corrid_funcs; /**< The path to a file containing functions for which the correlation ID counter should be ignored. If an empty string (default) no IDs will be ignored*/

    ChimbukoParams();

    void print() const;
  };


  /**
   * @brief The main interface for the AD module
   */
  class Chimbuko{
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
     * @brief Whether the parameter server is in use
     */
    bool use_ps() const { return m_outlier->use_ps(); }

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
    int get_step() const { return m_parser->getCurrentStep(); }

    /**
     * @brief Run the Chimbuko analysis for a single IO frame/step
     * @param[out] n_func_events number of function events recorded (incremented)
     * @param[out] n_comm_events number of comm events recorded (incremented)
     * @param[out] n_counter_events number of counter events recorded (incremented)
     * @param[out] n_outlier number of anomalous events recorded (incremented)
     * @return True if an IO frame was read. A false value indicates the end of the stream. (The "get_status" method provides the same value)
     */
    bool runFrame(unsigned long long& n_func_events,
		  unsigned long long& n_comm_events,
		  unsigned long long& n_counter_events,
		  unsigned long& n_outliers);

    /**
     * @brief Run the main Chimbuko analysis loop
     * @param[out] n_func_events number of function events recorded
     * @param[out] n_comm_events number of comm events recorded
     * @param[out] n_counter_events number of counter events recorded
     * @param[out] n_outlier number of anomalous events recorded
     * @param[out] frames number of adios2 input steps
     */
    void run(unsigned long long& n_func_events,
	     unsigned long long& n_comm_events,
	     unsigned long long& n_counter_events,
	     unsigned long& n_outliers,
	     unsigned long& frames);
    

  private:
    //Initialize various components; called by initialize method
    void init_io();
    void init_parser();
    void init_event();

    void init_net_client();
    void init_outlier();
    void init_counter();

#ifdef ENABLE_PROVDB
    void init_provdb();
#endif
    void init_provenance_gatherer();
    void init_metadata_parser();
    void init_monitoring();

    /**
     * @brief Signal the parser to parse the adios2 timestep
     * @param[out] step index
     * @param[out] number of func events parsed
     * @param[out] number of comm events parsed
     * @param[out] number of counter events parsed
     * @return false if unsuccessful, true otherwise
     */
    bool parseInputStep(int &step,
			unsigned long long& n_func_events,
			unsigned long long& n_comm_events,
			unsigned long long& n_counter_event);


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
     * @brief Extract provenance information about anomalies and communicate to provenance DB
     */
    void extractAndSendProvenance(const Anomalies &anomalies,
				  const int step,
				  const unsigned long first_event_ts,
				  const unsigned long last_event_ts) const;


    /**
     * @brief Gather and send the required data to the pserver
     */
    void gatherAndSendPSdata(const Anomalies &anomalies,
			     const int step,
			     const unsigned long first_event_ts,
			     const unsigned long last_event_ts) const;

    /**
     * @brief Send new metadata entries collected during current fram to provenance DB
     */
    void sendNewMetadataToProvDB(int step) const;

    //Components and parameters
    ADParser * m_parser;       /**< adios2 input data stream parser */
    ADEvent * m_event;         /**< func/comm event manager */
    ADCounter * m_counter;     /**< counter event manager */
    ADOutlier * m_outlier;     /**< outlier detection algorithm */
    ADio * m_io;               /**< output writer */
    ADThreadNetClient * m_net_client; /**< client for comms with parameter server */
    ADMetadataParser *m_metadata_parser; /**< parser for metadata */
#ifdef ENABLE_PROVDB
    ADProvenanceDBclient *m_provdb_client; /**< provenance DB client*/
#endif
    ADAnomalyProvenance  *m_anomaly_provenance; /**< provenance information gatherer*/ 

    ADMonitoring *m_monitoring; /**< maintain the node state by parsing counters from TAU's monitoring plugin*/

    PointerRegistry m_ptr_registry; /**< Managers pointers to components ensuring they are erased in the correct order*/

    mutable PerfStats m_perf; /**< Performance data */
    mutable PerfPeriodic m_perf_prd; /**<Performance temporal logging */

    ChimbukoParams m_params; /**< Parameters to setup the AD*/
    bool m_is_initialized; /**< Whether the AD has been initialized*/

    //State of currently stored function executions
    unsigned long m_execdata_first_event_ts; /**< earliest timestamp in current execution data */
    unsigned long m_execdata_last_event_ts; /**< latest timestamp in current execution data */
    bool m_execdata_first_event_ts_set; /**< False if the first event ts has not yet been set*/

    //State of accumulated statistics
    unsigned long long m_n_func_events_accum_prd; /**< Total number of function events since last write of periodic data*/
    unsigned long long m_n_comm_events_accum_prd; /**< Total number of comm events since last write of periodic data*/
    unsigned long long m_n_counter_events_accum_prd; /**< Total number of counter events since last write of periodic data*/
    unsigned long m_n_outliers_accum_prd; /**< Total number of outiers detected since last write of periodic data*/
    int m_n_steps_accum_prd; /**< Number of steps since last write of periodic data */

    std::set<unsigned long> m_exec_ignore_counters; /**< Counter indices in this list are ignored by the event manager (but will still be picked up by other components)*/

    ADExecDataInterface::FunctionsSeenType m_func_seen; /**< If using SST algorithm, we record whether a function has previously been seen in order to avoid including the first call to a function in the data*/
  };

}
