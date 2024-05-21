#include<chimbuko_config.h>
#ifdef USE_MPI
#include<mpi.h>
#endif
#include "chimbuko/core/chimbuko.hpp"
#include "chimbuko/core/verbose.hpp"
#include "chimbuko/core/util/string.hpp"
#include "chimbuko/core/util/commandLineParser.hpp"
#include "chimbuko/core/util/error.hpp"
#include "chimbuko/modules/performance_analysis/module.hpp"
#include <chrono>
#include <cstdlib>


using namespace chimbuko;
using namespace std::chrono;

void baseOptArgs(commandLineParser &parser, ChimbukoParams &into){
  addOptionalCommandLineArg(parser, into.base_params, ad_algorithm="hbos", "Set an AD algorithm to use: hbos or sstd (default \"hbos\").");
  addOptionalCommandLineArg(parser, into.base_params, hbos_threshold=0.99, "Set Threshold for HBOS anomaly detection filter (default 0.99).");
  addOptionalCommandLineArg(parser, into.base_params, hbos_use_global_threshold=true, "Set true to use a global threshold in HBOS algorithm (default true).");
  addOptionalCommandLineArg(parser, into.base_params, hbos_max_bins=200, "Set the maximum number of bins for histograms in the HBOS algorithm (default 200).");
  addOptionalCommandLineArgOptArg(parser, into.base_params, ana_obj_idx=0, program_idx, "Set the index associated with the instrumented program. Use to label components of a workflow. (default 0)");
  addOptionalCommandLineArg(parser, into.base_params, outlier_sigma=6.0, "Set the number of standard deviations that defines an anomalous event (default 6)");
  addOptionalCommandLineArg(parser, into.base_params, net_recv_timeout=30000, "Timeout (in ms) for blocking receives on client from parameter server (default 30000)");
  addOptionalCommandLineArg(parser, into.base_params, pserver_addr="", "Set the address of the parameter server. If empty (default) the pserver will not be used.");
  addOptionalCommandLineArg(parser, into.base_params, hpserver_nthr=1, "Set the number of threads used by the hierarchical PS. This parameter is used to compute a port offset for the particular endpoint that this AD rank connects to (default 1)");
  addOptionalCommandLineArg(parser, into.base_params, interval_msec=0, "Force the AD to pause for this number of ms at the end of each IO step (default 0)");
  addOptionalCommandLineArg(parser, into.base_params, prov_outputpath="", "Output provenance data to this directory. Can be used in place of or in conjunction with the provenance database. An empty string \"\" (default) disables this output");
#ifdef ENABLE_PROVDB
  addOptionalCommandLineArg(parser, into.base_params, provdb_addr_dir="", "Directory in which the provenance database outputs its address files. If empty (default) the provenance DB will not be used.");
  addOptionalCommandLineArg(parser, into.base_params, nprovdb_shards=1, "Number of provenance database shards. Clients connect to shards round-robin by rank (default 1)");
  addOptionalCommandLineArg(parser, into.base_params, nprovdb_instances=1, "Number of provenance database instances. Shards are divided uniformly over instances. (default 1)");
  addOptionalCommandLineArg(parser, into.base_params, provdb_mercury_auth_key="", "Set the Mercury authorization key for connection to the provDB (default \"\")");
#endif
#ifdef _PERF_METRIC
  addOptionalCommandLineArg(parser, into.base_params, perf_outputpath="", "Output path for AD performance monitoring data. If an empty string (default) no output is written.");
  addOptionalCommandLineArg(parser, into.base_params, perf_step=10, "How frequently (in IO steps) the performance data is dumped (default 10)");
#endif
  addOptionalCommandLineArg(parser, into.base_params, err_outputpath="", "Directory in which to place error logs. If an empty string (default) the errors will be piped to std::cerr");
#ifdef USE_MPI
  addOptionalCommandLineArg(parser, into.base_params, rank=-1234, "Set the rank index of the trace data. Used for verification unless override_rank is set. A value < 0 signals the value to be equal to the MPI rank of Chimbuko driver (default)");
#else
  addOptionalCommandLineArg(parser, into.base_params, rank=-1234, "Set the rank index of the trace data (default 0)");
#endif
  addOptionalCommandLineArg(parser, into.base_params, global_model_sync_freq=1, "Set the frequency in steps between updates of the global model (default 1)");
  addOptionalCommandLineArg(parser, into.base_params, ps_send_stats_freq=1, "Set how often in steps the statistics data is uploaded to the pserver (default 1)");
  addOptionalCommandLineArg(parser, into.base_params, step_report_freq=1, "Set the steps between Chimbuko reporting IO step progress. Use 0 to deactivate this logging entirely (default 1)");
  addOptionalCommandLineArg(parser, into.base_params, prov_record_startstep=-1, "If != -1, the IO step on which to start recording provenance information for anomalies (for testing, default -1)");
  addOptionalCommandLineArg(parser, into.base_params, prov_record_stopstep=-1, "If != -1, the IO step on which to stop recording provenance information for anomalies (for testing, default -1)");
  addOptionalCommandLineArg(parser, into.base_params, prov_io_freq=1, "Set the frequency in steps at which provenance data is writen/sent to the provDB (default 1)");
  addOptionalCommandLineArg(parser, into.base_params, analysis_step_freq=1, "Set the frequency in IO steps between analyzing the data. Data will be accumulated over intermediate steps. (default 1)");
  parser.addOptionalArg(progressHeadRank()=0, "-logging_head_rank", "Set the head rank upon which progress logging will be output (default 0)");
}


void mainOptArgs(commandLineParser &parser, ChimbukoParams &into, int &input_data_rank){
  addOptionalCommandLineArg(parser, into, func_threshold_file="", "Provide the path to a file containing HBOS/COPOD algorithm threshold overrides for specified functions. Format is JSON: \"[ { \"fname\": <FUNC>, \"threshold\": <THRES> },... ]\". Empty string (default) uses default threshold for all funcs");
  addOptionalCommandLineArg(parser, into, ignored_func_file="", "Provide the path to a file containing function names (one per line) which the AD algorithm will ignore. All such events are labeled as normal. Empty string (default) performs AD on all events.");
  addOptionalCommandLineArg(parser, into, anom_win_size=10, "When anomaly data are recorded a window of this size (in units of function execution events) around the anomalous event are also recorded (default 10)");
  addOptionalCommandLineArg(parser, into, trace_connect_timeout=60, "(For SST mode) Set the timeout in seconds on the connection to the TAU-instrumented binary (default 60s)");
  addOptionalCommandLineArg(parser, into, outlier_statistic="exclusive_runtime", "Set the statistic used for outlier detection. Options: exclusive_runtime (default), inclusive_runtime");
  addOptionalCommandLineArg(parser, into, prov_min_anom_time=0, "Set the minimum exclusive runtime (in microseconds) for anomalies to recorded in the provenance output (default 0)");
  addOptionalCommandLineArg(parser, into, monitoring_watchlist_file="", "Provide a filename containing the counter watchlist for the integration with the monitoring plugin. Empty string (default) uses the default subset. File format is JSON: \"[ [<COUNTER NAME>, <FIELD NAME>], ... ]\" where COUNTER NAME is the name of the counter in the input data stream and FIELD NAME the name of the counter in the provenance output.");
  addOptionalCommandLineArg(parser, into, monitoring_counter_prefix="", "Provide an optional prefix marking a set of monitoring plugin counters to be captured, on top of or superseding the watchlist. Empty string (default) is ignored.");
  addOptionalCommandLineArg(parser, into, parser_beginstep_timeout=30, "Set the timeout in seconds on waiting for the next ADIOS2 timestep (default 30s)");
  parser.addOptionalArgWithFlag(input_data_rank, into.override_rank=false, "-override_rank", "Set Chimbuko to overwrite the rank index in the parsed data with its own internal rank parameter. The value provided should be the original rank index of the data. This disables verification of the data rank.");
}

void printHelp(){
  std::cout << "Usage: driver <Trace engine type> <Trace directory> <Trace file prefix> <Options>\n"
	    << "Where <Trace engine type> : BPFile or SST\n"
	    << "      <Trace directory>   : The directory in which the BPFile or SST file is located\n"
	    << "      <Trace file prefix> : The prefix of the file (the trace file name without extension e.g. \"tau-metrics-mybinary\" for \"tau-metrics-mybinary.bp\")\n"
	    << "      <Options>           : Optional arguments as described below.\n";
  commandLineParser p; ChimbukoParams pp; int r;
  baseOptArgs(p, pp);
  mainOptArgs(p, pp, r);
  p.help(std::cout);
}

ChimbukoParams getParamsFromCommandLine(int argc, char** argv
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
  ChimbukoParams params;

  //Parameters for the connection to the instrumented binary trace output
  params.trace_engineType = argv[1]; // BPFile or SST
  params.trace_data_dir = argv[2]; // *.bp location
  std::string bp_prefix = argv[3]; // bp file prefix (e.g. tau-metrics-[nwchem])

  commandLineParser parser;
  int input_data_rank;
  baseOptArgs(parser, params);
  mainOptArgs(parser, params, input_data_rank);
  
  parser.parse(argc-4, (const char**)(argv+4));

  //By default assign the rank index of the trace data as the MPI rank of the AD process
  //Allow override by user
  if(params.base_params.rank < 0){
#ifdef USE_MPI
    params.base_params.rank = mpi_world_rank;
#else
    params.base_params.rank = 0; //default to 0 for non-MPI applications
#endif
  }

  params.base_params.verbose = params.base_params.rank == 0; //head node produces verbose output

  //Assume the rank index of the data is the same as the driver rank parameter
  params.trace_inputFile = bp_prefix + "-" + std::to_string(params.base_params.rank) + ".bp";

  //If we are forcing the parsed data rank to match the driver rank parameter, this implies it was not originally
  //Thus we need to obtain the input data rank also from the command line and modify the filename accordingly
  if(params.override_rank)
    params.trace_inputFile = bp_prefix + "-" + std::to_string(input_data_rank) + ".bp";

  //If neither the provenance database or the provenance output path are set, default to outputting to pwd
  if(params.base_params.prov_outputpath.size() == 0
#ifdef ENABLE_PROVDB
     && params.base_params.provdb_addr_dir.size() == 0
#endif
     ){
    params.base_params.prov_outputpath = ".";
  }

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
  ChimbukoParams params = getParamsFromCommandLine(argc, argv
#ifdef USE_MPI
    , mpi_world_rank
#endif
    );

  int rank = params.base_params.rank;

  if(rank == progressHeadRank()) params.print();

  if(enableVerboseLogging()){
    headProgressStream(rank) << "Driver rank " << rank << ": Enabling verbose debug output" << std::endl;
  }

  verboseStream << "Driver rank " << rank << ": waiting at pre-run barrier" << std::endl;
#ifdef USE_MPI
  MPI_Barrier(MPI_COMM_WORLD);
#endif

  bool error = false;

  try
    {
      //Instantiate Chimbuko
      Chimbuko driver(params);

      // -----------------------------------------------------------------------
      // Measurement variables
      // -----------------------------------------------------------------------
      unsigned long total_frames = 0, frames = 0;
      unsigned long total_n_outliers = 0, n_outliers = 0;
      unsigned long total_processing_time = 0, processing_time = 0;
      unsigned long long n_func_events = 0, n_comm_events = 0, n_counter_events = 0;
      unsigned long long total_n_func_events = 0, total_n_comm_events = 0, total_n_counter_events = 0;
      high_resolution_clock::time_point t1, t2;

      // -----------------------------------------------------------------------
      // Start analysis
      // -----------------------------------------------------------------------
      headProgressStream(rank) << "Driver rank " << rank << ": analysis start" << std::endl;

      t1 = high_resolution_clock::now();
      //driver.run(n_func_events,
      //n_comm_events,
      //n_counter_events
      //n_outliers,
      //		 frames);
      driver.run();
      t2 = high_resolution_clock::now();

      if (rank == 0) {
	headProgressStream(rank) << "Driver rank " << rank << ": analysis done!\n";
	driver.show_status(true);
      }

      // -----------------------------------------------------------------------
      // Average analysis time and total number of outliers
      // -----------------------------------------------------------------------
#ifdef USE_MPI
      verboseStream << "Driver rank " << rank << ": waiting at post-run barrier" << std::endl;
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


      headProgressStream(rank) << "Driver rank " << rank << ": Final report\n"
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
    std::cout << '[' << getDateTime() << ", rank " << rank << "] Driver : caught invalid argument: " << e.what() << std::endl;
    if(params.base_params.err_outputpath.size()) recoverable_error(std::string("Driver : caught invalid argument: ") + e.what()); //ensure errors also written to error logs
    error = true;
  }
  catch (const std::ios_base::failure &e){
    std::cout << '[' << getDateTime() << ", rank " << rank << "] Driver : I/O base exception caught: " << e.what() << std::endl;
    if(params.base_params.err_outputpath.size()) recoverable_error(std::string("Driver : I/O base exception caught: ") + e.what());
    error = true;
  }
  catch (const std::exception &e){
    std::cout << '[' << getDateTime() << ", rank " << rank << "] Driver : Exception caught: " << e.what() << std::endl;
    if(params.base_params.err_outputpath.size()) recoverable_error(std::string("Driver : Exception caught: ") + e.what());
    error = true;
  }

#ifdef USE_MPI
  MPI_Finalize();
#endif
  headProgressStream(rank) << "Driver is exiting" << std::endl;
  return error ? 1 : 0;
}
