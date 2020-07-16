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

    //Parameters associated with the outlier detection algorithm
    double outlier_sigma; /**< The number of sigma (standard deviations) away from the mean runtime for an event to be considered anomalous */


    //Parameters associated with communicating with the parameter server*/
    std::string pserver_addr; /**< The address of the parameter server. 
				 < If no parameter server is in use, this string should be empty (length zero)
				 < If using ZmqNet (default) this is a tcp address of the form "tcp://${ADDRESS}:${PORT}" 
			      */


    //Parameters associated with communicating with the visualization (viz) module
    IOMode viz_iomode; /**< Set to IOMode::Online to send to viz module, IOMode::Offline to dump to disk, or IOMode::Both for both */
    std::string viz_datadump_outputPath; /**< If writing to disk, write to this directory */
    std::string viz_addr; /**< If sending to the viz module, this is the web address (expected http://....) */
    unsigned int viz_anom_winSize; /**< When anomaly data are written, a window of this size (in units of events) around the anomalous event are also sent */

#ifdef ENABLE_PROVDB
    //Parameters associated with the provenance database
    std::string provdb_addr; /**< Address of the provenance database. If empty the provenance DB will not be used.
				< Has format "ofi+tcp;ofi_rxm://${IP_ADDR}:${PORT}". Should also accept "tcp://${IP_ADDR}:${PORT}" */
#endif

    //Parameters associated with performance analysis of AD module
    std::string perf_outputpath; /**< Output path for AD performance monitoring data. If an empty string no output is written.*/
    int perf_step; /**<How frequently (in IO steps) the performance data is dumped*/

    //General parameters for Chimbuko
    int rank; /**< MPI rank of AD process */
    bool verbose; /**< Enable verbose output. Typically one enables this only on a single node (eg verbose = (rank==0); ) */
    bool only_one_frame; /**< Force the AD to stop after a single IO frame */
    int interval_msec; /**< Force the AD to pause for this number of ms at the end of each IO step*/

    ChimbukoParams(): rank(-1234),  //not set!
		      verbose(true),
		      outlier_sigma(6.),
		      trace_engineType("BPFile"), trace_data_dir("."), trace_inputFile("TAU_FILENAME-BINARYNAME"),
		      pserver_addr(""),
		      viz_iomode(IOMode::Offline), viz_datadump_outputPath("."), viz_addr(""), viz_anom_winSize(0),
#ifdef ENABLE_PROVDB
		      provdb_addr(""),
#endif
		      perf_outputpath(""), perf_step(10),
		      only_one_frame(false), interval_msec(0)		      
    {}

    void print() const;
  };


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
     * @param rank The MPI rank of the process
     * @param step The adios2 stream step index
     */
    void extractEvents(int rank, int step);

    /**
     * @brief Extract parsed counters and insert into counter manager
     * @param rank The MPI rank of the process
     * @param step The adios2 stream step index
     */
    void extractCounters(int rank, int step);


#ifdef ENABLE_PROVDB
    /**
     * @brief Extract provenance information about anomalies and communicate to provenance DB
     */
    void extractAndSendProvenance(const Anomalies &anomalies) const;

    
    /**
     * @brief Send new metadata entries collected during current fram to provenance DB
     */
    void sendNewMetadataToProvDB() const;
#endif
    
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
#endif
    PerfStats m_perf;
    
    ChimbukoParams m_params; /**< Parameters to setup the AD*/
    bool m_is_initialized; /**< Whether the AD has been initialized*/
  };

}
