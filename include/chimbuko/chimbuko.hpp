#pragma once

#include "chimbuko/AD.hpp"

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


    //Parameters associated with the outlier detection algorithm
    double outlier_sigma; /**< The number of sigma (standard deviations) away from the mean runtime for an event to be considered anomalous */


    //Parameters associated with communicating with the parameter server*/
    std::string pserver_addr; /**< The address of the parameter server. 
				 < If no parameter server is in use, this string should be empty (length zero)
				 < If using ZmqNet (default) this is a tcp address of the form "tcp://${ADDRESS}:${PORT}" 
			      */
    int hpserver_nthr;        /**< If using the hierarchical pserver, this parameter is used to compute a port offset for the particular endpoint that this AD rank connects to */

    std::string provdata_outdir; /**< Directory where provenance data is written (in conjunction with provDB if active). Blank string indicates no output*/
#ifdef ENABLE_PROVDB
    //Parameters associated with the provenance database
    std::string provdb_addr; /**< Address of the provenance database. If empty the provenance DB will not be used.
				< Has format "ofi+tcp;ofi_rxm://${IP_ADDR}:${PORT}". Should also accept "tcp://${IP_ADDR}:${PORT}" */
    int nprovdb_shards; /**< Number of database shards*/
#endif

    unsigned int anom_win_size; /**< When anomaly data are recorded, a window of this size (in units of events) around the anomalous event are also recorded (used both for viz and provDB)*/

    //Parameters associated with performance analysis of AD module
    std::string perf_outputpath; /**< Output path for AD performance monitoring data. If an empty string no output is written.*/
    int perf_step; /**<How frequently (in IO steps) the performance data is dumped*/

    //General parameters for Chimbuko
    int program_idx; /**< Program index (for workflows with >1 component) */
    int rank; /**< MPI rank of AD process */
    bool verbose; /**< Enable verbose output. Typically one enables this only on a single node (eg verbose = (rank==0); ) */
    bool only_one_frame; /**< Force the AD to stop after a single IO frame */
    int interval_msec; /**< Force the AD to pause for this number of ms at the end of each IO step*/
    std::string err_outputpath; /**< Output path for error logs. If empty errors will be sent to std::cerr*/
    int parser_beginstep_timeout; /**< Set the timeout in seconds on waiting for the next ADIOS2 timestep (default 30s)*/


    ChimbukoParams(): rank(-1234),  //not set!
		      program_idx(0),
		      verbose(true),
		      outlier_sigma(6.),
		      trace_engineType("BPFile"), trace_data_dir("."), trace_inputFile("TAU_FILENAME-BINARYNAME"),
		      trace_connect_timeout(60),
		      pserver_addr(""), hpserver_nthr(1),
		      anom_win_size(10),
#ifdef ENABLE_PROVDB
		      provdb_addr(""), nprovdb_shards(1),
#endif
		      perf_outputpath(""), perf_step(10),
		      only_one_frame(false), interval_msec(0),
		      err_outputpath(""), parser_beginstep_timeout(30)
    {}

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

    void init_metadata_parser();


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
     * @brief Extract provenance information about anomalies and communicate to provenance DB
     */
    void extractAndSendProvenance(const Anomalies &anomalies,
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
    ADOutlierSSTD * m_outlier; /**< outlier detection algorithm */
    ADio * m_io;               /**< output writer */
    ADNetClient * m_net_client; /**< client for comms with parameter server */
    ADMetadataParser *m_metadata_parser; /**< parser for metadata */
#ifdef ENABLE_PROVDB
    ADProvenanceDBclient *m_provdb_client; /**< provenance DB client*/
    ADNormalEventProvenance *m_normalevent_prov; /**< maintain provenance info of normal events*/
#endif
    mutable PerfStats m_perf; /**< Performance data */
    mutable PerfPeriodic m_perf_prd; /**<Performance temporal logging */
    
    ChimbukoParams m_params; /**< Parameters to setup the AD*/
    bool m_is_initialized; /**< Whether the AD has been initialized*/
  };

}
