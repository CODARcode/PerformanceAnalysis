#include<chimbuko_config.h>
#ifdef USE_MPI
#include<mpi.h>
#endif
#include "chimbuko/chimbuko.hpp"
#include "chimbuko/verbose.hpp"
#include "chimbuko/util/string.hpp"
#include "chimbuko/util/commandLineParser.hpp"
#include "chimbuko/util/error.hpp"
#include <chrono>
#include <cstdlib>

using namespace chimbuko;
using namespace std::chrono;

struct ChimbukoMultiRankParams : public ChimbukoParams{
  int nranks; //number of ranks handled by this instance
  int base_rank; //base index of ranks handled by this instance (assumed contiguous). Value <0 uses the MPI rank.

  ChimbukoMultiRankParams(): nranks(1), base_rank(-1), ChimbukoParams(){}
};


typedef commandLineParser<ChimbukoMultiRankParams> optionalArgsParser;

//Specialized class for setting the global head rank for logging purposes (doesn't actually set a parameter in the struct)
struct setLoggingHeadRankArg: public optionalCommandLineArgBase<ChimbukoMultiRankParams>{
  int parse(ChimbukoMultiRankParams &into, const std::string &arg, const char** vals, const int vals_size){
    if(arg == "-logging_head_rank"){
      if(vals_size < 1) return -1;
      int v;
      try{
	v = strToAny<int>(vals[0]);
      }catch(const std::exception &exc){
	return -1;
      }
      progressHeadRank() = v;
      verboseStream << "Driver: Set global head rank for log output to " << v << std::endl;
      return 1;
    }
    return -1;
  }
  void help(std::ostream &os) const{
    os << "-logging_head_rank : Set the head rank upon which progress logging will be output (default 0)";
  }
};



optionalArgsParser & getOptionalArgsParser(){
  static bool initialized = false;
  static optionalArgsParser p;
  if(!initialized){
    addOptionalCommandLineArg(p, ad_algorithm, "Set an AD algorithm to use: hbos or sstd.");
    addOptionalCommandLineArg(p, hbos_threshold, "Set Threshold for HBOS anomaly detection filter.");
    addOptionalCommandLineArg(p, hbos_use_global_threshold, "Set true to use a global threshold in HBOS algorithm. Default is true.");
    addOptionalCommandLineArg(p, hbos_max_bins, "Set the maximum number of bins for histograms in the HBOS algorithm. Default 200.");
    addOptionalCommandLineArg(p, program_idx, "Set the index associated with the instrumented program. Use to label components of a workflow. (default 0)");
    addOptionalCommandLineArg(p, outlier_sigma, "Set the number of standard deviations that defines an anomalous event (default 6)");
    addOptionalCommandLineArg(p, func_threshold_file, "A filename containing HBOS/COPOD algorithm threshold overrides for specified functions. Format is JSON: \"[ { \"fname\": <FUNC>, \"threshold\": <THRES> },... ]\". Empty string (default) uses default threshold for all funcs");
    addOptionalCommandLineArg(p, ignored_func_file, "Provide the path to a file containing function names (one per line) which the AD algorithm will ignore. All such events are labeled as normal. Empty string (default) performs AD on all events.");
    addOptionalCommandLineArg(p, net_recv_timeout, "Timeout (in ms) for blocking receives on client from parameter server (default 30000)");
    addOptionalCommandLineArg(p, pserver_addr, "Set the address of the parameter server. If empty (default) the pserver will not be used.");
    addOptionalCommandLineArg(p, hpserver_nthr, "Set the number of threads used by the hierarchical PS. This parameter is used to compute a port offset for the particular endpoint that this AD rank connects to (default 1)");
    addOptionalCommandLineArg(p, interval_msec, "Force the AD to pause for this number of ms at the end of each IO step (default 0)");
    addOptionalCommandLineArg(p, anom_win_size, "When anomaly data are recorded a window of this size (in units of function execution events) around the anomalous event are also recorded (default 10)");
    addOptionalCommandLineArg(p, prov_outputpath, "Output provenance data to this directory. Can be used in place of or in conjunction with the provenance database. An empty string \"\" (default) disables this output");
#ifdef ENABLE_PROVDB
    addOptionalCommandLineArg(p, provdb_addr_dir, "Directory in which the provenance database outputs its address files. If empty (default) the provenance DB will not be used.");
    addOptionalCommandLineArg(p, nprovdb_shards, "Number of provenance database shards. Clients connect to shards round-robin by rank (default 1)");
    addOptionalCommandLineArg(p, nprovdb_instances, "Number of provenance database instances. Shards are divided uniformly over instances. (default 1)");
#endif
#ifdef _PERF_METRIC
    addOptionalCommandLineArg(p, perf_outputpath, "Output path for AD performance monitoring data. If an empty string (default) no output is written.");
    addOptionalCommandLineArg(p, perf_step, "How frequently (in IO steps) the performance data is dumped (default 10)");
#endif
    addOptionalCommandLineArg(p, err_outputpath, "Directory in which to place error logs. If an empty string (default) the errors will be piped to std::cerr");
    addOptionalCommandLineArg(p, trace_connect_timeout, "(For SST mode) Set the timeout in seconds on the connection to the TAU-instrumented binary (default 60s)");
    addOptionalCommandLineArg(p, parser_beginstep_timeout, "Set the timeout in seconds on waiting for the next ADIOS2 timestep (default 30s)");

    p.addOptionalArg(new setLoggingHeadRankArg); //-logging_head_rank <rank>

    addOptionalCommandLineArg(p, outlier_statistic, "Set the statistic used for outlier detection. Options: exclusive_runtime (default), inclusive_runtime");
    addOptionalCommandLineArg(p, step_report_freq, "Set the steps between Chimbuko reporting IO step progress. Use 0 to deactivate this logging entirely (default 1)");
    addOptionalCommandLineArg(p, prov_record_startstep, "If != -1, the IO step on which to start recording provenance information for anomalies (for testing, default -1)");
    addOptionalCommandLineArg(p, prov_record_stopstep, "If != -1, the IO step on which to stop recording provenance information for anomalies (for testing, default -1)");
    addOptionalCommandLineArg(p, analysis_step_freq, "Set the frequency in IO steps between analyzing the data. Data will be accumulated over intermediate steps. (default 1)");      
    addOptionalCommandLineArg(p, read_ignored_corrid_funcs, "Set path to a file containing functions (one per line) for which the correlation ID counter should be ignored. If an empty string (default) no IDs will be ignored");
    addOptionalCommandLineArg(p, monitoring_watchlist_file, "Provide a filename containing the counter watchlist for the integration with the monitoring plugin. Empty string (default) uses the default subset. File format is JSON: \"[ [<COUNTER NAME>, <FIELD NAME>], ... ]\" where COUNTER NAME is the name of the counter in the input data stream and FIELD NAME the name of the counter in the provenance output.");
    addOptionalCommandLineArg(p, max_frames, "Stop analyzing data once this number of IO frames have been read. A value < 0 (default) is unlimited");

    addOptionalCommandLineArg(p, nranks, "Set the number of ranks handled by this instance (default 1)");

    addOptionalCommandLineArg(p, base_rank, 
#ifdef USE_MPI
			      "Set the base rank index of the trace data. A value < 0 signals the value to be equal to the nranks * (MPI rank of driver) (default)"
#else
			      "Set the rank index of the trace data (default 0)"
#endif
			      );

    initialized = true;
  }
  return p;
};

void printHelp(){
  std::cout << "Usage: driver_multirank <Trace engine type> <Trace directory> <Trace file prefix> <Options>\n"
	    << "Where <Trace engine type> : BPFile or SST\n"
	    << "      <Trace directory>   : The directory in which the BPFile or SST file is located\n"
	    << "      <Trace file prefix> : The prefix of the file (the trace file name without extension e.g. \"tau-metrics-mybinary\" for \"tau-metrics-mybinary.bp\")\n"
	    << "      <Options>           : Optional arguments as described below.\n";
  getOptionalArgsParser().help(std::cout);
}

ChimbukoMultiRankParams getParamsFromCommandLine(int argc, char** argv
#ifdef USE_MPI
, const int mpi_world_rank
#endif
						 ){
  if(argc < 4){
    std::cerr << "Expected at least 4 arguments: <exe> <BPFile/SST> <.bp location> <bp file prefix>" << std::endl;
    exit(-1);
  }

  // -----------------------------------------------------------------------
  // Parse command line arguments (cf chimbuko.hpp for detailed description of parameters)
  // -----------------------------------------------------------------------
  ChimbukoMultiRankParams params;

  //Parameters for the connection to the instrumented binary trace output
  params.trace_engineType = argv[1]; // BPFile or SST
  params.trace_data_dir = argv[2]; // *.bp location
  params.trace_inputFile = argv[3]; // Need to append  -${rank}.bp later
  
  getOptionalArgsParser().parse(params, argc-4, (const char**)(argv+4));

  //If neither the provenance database or the provenance output path are set, default to outputting to pwd
  if(params.prov_outputpath.size() == 0
#ifdef ENABLE_PROVDB
     && params.provdb_addr_dir.size() == 0
#endif
     ){
    params.prov_outputpath = ".";
  }

  //Handle the base rank
  if(params.nranks < 1) fatal_error("Number of ranks must be at least 1");

#ifdef USE_MPI
  if(params.base_rank < 0) params.base_rank = params.nranks * mpi_world_rank;
#else
  if(params.base_rank < 0) fatal_error("If used in non-MPI mode, the base rank must be set");
#endif

  return params;
}




int main(int argc, char ** argv){
  if(argc == 1 || (argc == 2 && std::string(argv[1]) == "-help") ){
    printHelp();
    return 0;
  }

#ifdef USE_MPI
  assert( MPI_Init(&argc, &argv) == MPI_SUCCESS );

  int mpi_world_rank, mpi_world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_world_size);
#endif

  //Parse environment variables
  if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE"))
    enableVerboseLogging() = true;

  //Parse Chimbuko parameters
  ChimbukoMultiRankParams params = getParamsFromCommandLine(argc, argv
#ifdef USE_MPI
    , mpi_world_rank
#endif
    );

  if(params.base_rank == progressHeadRank()) params.print();

  if(enableVerboseLogging()){
    headProgressStream(params.base_rank) << "Multi-rank driver, base rank " << params.base_rank << ": Enabling verbose debug output" << std::endl;
  }

  verboseStream << "Multi-rank driver, base rank " << params.base_rank << ": waiting at pre-run barrier" << std::endl;
#ifdef USE_MPI
  MPI_Barrier(MPI_COMM_WORLD);
#endif

  bool error = false;

  // -----------------------------------------------------------------------
  // Measurement variables
  // -----------------------------------------------------------------------
  unsigned long total_frames = 0, frames = 0;
  unsigned long total_n_outliers = 0, n_outliers = 0;
  unsigned long total_processing_time = 0, processing_time = 0;
  unsigned long long n_func_events = 0, n_comm_events = 0, n_counter_events = 0;
  unsigned long long total_n_func_events = 0, total_n_comm_events = 0, total_n_counter_events = 0;
  high_resolution_clock::time_point t1, t2;

  try
    {
      ChimbukoParams const &base_params = dynamic_cast<ChimbukoParams const &>(params); //downcast to base to catch propagated parameters

      //Instantiate Chimbuko instances for each target rank
      std::vector<Chimbuko> drivers(params.nranks);
      for(int r=0;r<params.nranks;r++){
	ChimbukoParams rparams(base_params);
	rparams.rank = params.base_rank + r;
	rparams.trace_inputFile += "-" + std::to_string(rparams.rank) + ".bp";

	headProgressStream(params.base_rank) << "Multi-rank driver, base rank " << params.base_rank << ": initializing AD instance for data rank " << rparams.rank << " from stream " << rparams.trace_inputFile << std::endl;	
	drivers[r].initialize(rparams);
      }

      // -----------------------------------------------------------------------
      // Start analysis
      // -----------------------------------------------------------------------
      headProgressStream(params.base_rank) << "Multi-rank driver, base rank " << params.base_rank << ": analysis start"  << std::endl;

      t1 = high_resolution_clock::now();
      
      int ncomplete = 0;
      std::vector<unsigned long> rank_frames(params.nranks, 0); //frames completed by rank
      std::vector<bool> rank_enable(params.nranks, true); //enable analysis for the given rank
      for(int r=0;r<params.nranks;r++){
	if(params.max_frames == 0){ rank_enable[r] = false; ++ncomplete; }
      }

      while(ncomplete != params.nranks){
	for(int r=0;r<params.nranks;r++){
	  if(rank_enable[r] && drivers[r].get_status()){
	    bool did_frame = drivers[r].runFrame(n_func_events,
						 n_comm_events,
						 n_counter_events,
						 n_outliers);
	    if(did_frame){
	      ++frames;
	      ++rank_frames[r];

	      //Disable processing for rank if it has reached the max number of frames
	      if(params.max_frames > 0 && rank_frames[r] == params.max_frames){
		rank_enable[r] = false;
		++ncomplete;
	      }

	    }else{
	      if(drivers[r].get_status() == true) fatal_error("Logic bomb: no frame parsed but driver instance still connected to stream");
	      ++ncomplete;
	    }
	  }
	}
      }

      t2 = high_resolution_clock::now();

      if (params.base_rank == progressHeadRank() || enableVerboseLogging()) {
	headProgressStream(params.base_rank) << "Multi-rank driver base rank " << params.base_rank << ": analysis done!\n";
	for(int r=0;r<params.nranks;r++)
	  drivers[r].show_status(true);
      }

      // -----------------------------------------------------------------------
      // Average analysis time and total number of outliers
      // -----------------------------------------------------------------------
#ifdef USE_MPI
      verboseStream << "Multi-rank driver, base rank " << params.base_rank << ": waiting at post-run barrier" << std::endl;
      MPI_Barrier(MPI_COMM_WORLD);
      processing_time = duration_cast<milliseconds>(t2 - t1).count();

      {
	const unsigned long local_measures[] = {processing_time, n_outliers, frames};
	unsigned long global_measures[] = {0, 0, 0};
	MPI_Reduce(
		   local_measures, global_measures, 3, MPI_UNSIGNED_LONG,
		   MPI_SUM, 0, MPI_COMM_WORLD
		   );
	total_processing_time = global_measures[0];
	total_n_outliers = global_measures[1];
	total_frames = global_measures[2];
      }
      {
	const unsigned long long local_measures[] = {n_func_events, n_comm_events, n_counter_events};
	unsigned long long global_measures[] = {0, 0, 0};
	MPI_Reduce(
		   local_measures, global_measures, 3, MPI_UNSIGNED_LONG_LONG,
		   MPI_SUM, 0, MPI_COMM_WORLD
		   );
	total_n_func_events = global_measures[0];
	total_n_comm_events = global_measures[1];
	total_n_counter_events = global_measures[2];
      }
#else
      //Without MPI only report local parameters. In principle we could aggregate using the Pserver if we really want the global information
      total_processing_time = processing_time;
      total_n_outliers = n_outliers;
      total_frames = frames;

      total_n_func_events = n_func_events;
      total_n_comm_events = n_comm_events;
      total_n_counter_events = n_counter_events;
      
      int mpi_world_size = 1;
#endif


      headProgressStream(params.base_rank) << "Multi-rank driver, base rank " << params.base_rank << ": Final report\n"
				  << "Avg. num. frames over MPI ranks : " << (double)total_frames/(double)mpi_world_size << "\n"
				  << "Avg. processing time over MPI ranks : " << (double)total_processing_time/(double)mpi_world_size << " msec\n"
				  << "Total num. outliers  : " << total_n_outliers << "\n"
				  << "Total func events    : " << total_n_func_events << "\n"
				  << "Total comm events    : " << total_n_comm_events << "\n"
				  << "Total counter events    : " << total_n_counter_events << "\n"
				  << "Total function/comm events         : " << total_n_func_events + total_n_comm_events
				  << std::endl;
    }
  catch (const std::invalid_argument &e){
    std::cout << '[' << getDateTime() << ", base rank " << params.base_rank << "] Driver : caught invalid argument: " << e.what() << std::endl;
    if(params.err_outputpath.size()) recoverable_error(std::string("Driver : caught invalid argument: ") + e.what()); //ensure errors also written to error logs
    error = true;
  }
  catch (const std::ios_base::failure &e){
    std::cout << '[' << getDateTime() << ", base rank " << params.base_rank << "] Driver : I/O base exception caught: " << e.what() << std::endl;
    if(params.err_outputpath.size()) recoverable_error(std::string("Driver : I/O base exception caught: ") + e.what());
    error = true;
  }
  catch (const std::exception &e){
    std::cout << '[' << getDateTime() << ", base rank " << params.base_rank << "] Driver : Exception caught: " << e.what() << std::endl;
    if(params.err_outputpath.size()) recoverable_error(std::string("Driver : Exception caught: ") + e.what());
    error = true;
  }

#ifdef USE_MPI
  MPI_Finalize();
#endif
  headProgressStream(params.base_rank) << "Multi-rank driver, base rank " << params.base_rank << " is exiting" << std::endl;
  return error ? 1 : 0;
}
