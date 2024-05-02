#include<chimbuko_config.h>
#ifdef USE_MPI
#include<mpi.h>
#endif
#include "chimbuko/chimbuko.hpp"
#include "chimbuko/verbose.hpp"
#include "chimbuko/core/util/string.hpp"
#include "chimbuko/core/util/commandLineParser.hpp"
#include "chimbuko/core/util/error.hpp"
#include <chrono>
#include <cstdlib>


using namespace chimbuko;
using namespace std::chrono;

template<typename T>
bool optArg(T &into, const std::string &opt, char** argv_p){
  if(*argv_p == opt){
    try{
      into = strToAny<T>(*(argv_p+1));
    }catch(const std::exception &exc){
      return false;
    }
    return true;
  }else{
    return false;
  }
}

bool baseOptArgs(ChimbukoParams &into, char** argv_p){
#define OPT(A) if(optArg(into.base_params.A, "-" #A, argv_p)) return true;
  OPT(ad_algorithm);
  OPT(hbos_threshold);
  OPT(hbos_use_global_threshold);
  OPT(hbos_max_bins);
  if(optArg(into.base_params.ana_obj_idx, "-program_idx", argv_p)) return true;
  OPT(outlier_sigma);
  OPT(net_recv_timeout);
  OPT(pserver_addr);
  OPT(hpserver_nthr);
  OPT(interval_msec);
  OPT(prov_outputpath);
#ifdef ENABLE_PROVDB
  OPT(provdb_addr_dir);
  OPT(nprovdb_shards);
  OPT(nprovdb_instances);
  OPT(provdb_mercury_auth_key);
#endif
#ifdef _PERF_METRIC
  OPT(perf_outputpath);
  OPT(perf_step);
#endif
  OPT(err_outputpath);
  OPT(rank);
  OPT(global_model_sync_freq);
  OPT(ps_send_stats_freq);
  OPT(step_report_freq);
  OPT(prov_record_startstep);
  OPT(prov_record_stopstep);
  OPT(prov_io_freq);
  OPT(analysis_step_freq);

  if(std::string(*argv_p) == "-logging_head_rank"){
    try{
      int v = strToAny<int>(*(argv_p+1));
      progressHeadRank() = v;
      verboseStream << "Driver: Set global head rank for log output to " << v << std::endl;
      return true;
    }catch(const std::exception &exc){
    }
  }
    
  return false;
#undef OPT
}

void printHelpBase(){
#define HELP(CMD,STR) std::cout << "-" << #CMD << ": " << STR << std::endl;

  HELP(ad_algorithm, "Set an AD algorithm to use: hbos or sstd.");
  HELP(hbos_threshold, "Set Threshold for HBOS anomaly detection filter.");
  HELP(hbos_use_global_threshold, "Set true to use a global threshold in HBOS algorithm. Default is true.");
  HELP(hbos_max_bins, "Set the maximum number of bins for histograms in the HBOS algorithm. Default 200.");
  HELP(program_idx, "Set the index associated with the instrumented program. Use to label components of a workflow. (default 0)");
  HELP(outlier_sigma, "Set the number of standard deviations that defines an anomalous event (default 6)");
  HELP(net_recv_timeout, "Timeout (in ms) for blocking receives on client from parameter server (default 30000)");
  HELP(pserver_addr, "Set the address of the parameter server. If empty (default) the pserver will not be used.");
  HELP(hpserver_nthr, "Set the number of threads used by the hierarchical PS. This parameter is used to compute a port offset for the particular endpoint that this AD rank connects to (default 1)");
  HELP(interval_msec, "Force the AD to pause for this number of ms at the end of each IO step (default 0)");
  HELP(prov_outputpath, "Output provenance data to this directory. Can be used in place of or in conjunction with the provenance database. An empty string \"\" (default) disables this output");
#ifdef ENABLE_PROVDB
  HELP(provdb_addr_dir, "Directory in which the provenance database outputs its address files. If empty (default) the provenance DB will not be used.");
  HELP(nprovdb_shards, "Number of provenance database shards. Clients connect to shards round-robin by rank (default 1)");
  HELP(nprovdb_instances, "Number of provenance database instances. Shards are divided uniformly over instances. (default 1)");
  HELP(provdb_mercury_auth_key, "Set the Mercury authorization key for connection to the provDB (default \"\")");
#endif
#ifdef _PERF_METRIC
  HELP(perf_outputpath, "Output path for AD performance monitoring data. If an empty string (default) no output is written.");
  HELP(perf_step, "How frequently (in IO steps) the performance data is dumped (default 10)");
#endif
  HELP(err_outputpath, "Directory in which to place error logs. If an empty string (default) the errors will be piped to std::cerr");
#ifdef USE_MPI
  HELP(rank, "Set the rank index of the trace data. Used for verification unless override_rank is set. A value < 0 signals the value to be equal to the MPI rank of Chimbuko driver (default)");
#else
  HELP(rank, "Set the rank index of the trace data (default 0)");
#endif
  HELP(global_model_sync_freq, "Set the frequency in steps between updates of the global model (default 1)");
  HELP(ps_send_stats_freq, "Set how often in steps the statistics data is uploaded to the pserver (default 1)");
  HELP(step_report_freq, "Set the steps between Chimbuko reporting IO step progress. Use 0 to deactivate this logging entirely (default 1)");
  HELP(prov_record_startstep, "If != -1, the IO step on which to start recording provenance information for anomalies (for testing, default -1)");
  HELP(prov_record_stopstep, "If != -1, the IO step on which to stop recording provenance information for anomalies (for testing, default -1)");
  HELP(prov_io_freq, "Set the frequency in steps at which provenance data is writen/sent to the provDB (default 1)");
  HELP(analysis_step_freq, "Set the frequency in IO steps between analyzing the data. Data will be accumulated over intermediate steps. (default 1)");
  HELP(logging_head_rank, "Set the head rank upon which progress logging will be output (default 0)");
#undef HELP
}

bool mainOptArgs(ChimbukoParams &into, int &input_data_rank, char** argv_p){
#define OPT(A) if(optArg(into.A, "-" #A, argv_p)) return true;
  OPT(func_threshold_file);
  OPT(ignored_func_file);
  OPT(anom_win_size);
  OPT(trace_connect_timeout);
  OPT(outlier_statistic);
  OPT(prov_min_anom_time);
  OPT(monitoring_watchlist_file);
  OPT(monitoring_counter_prefix);
  OPT(parser_beginstep_timeout);
  
  if(std::string(*argv_p) == "-override_rank"){    
    try{
      int v = strToAny<int>(*(argv_p+1));
      input_data_rank = v;
      into.override_rank = true;
      verboseStream << "Driver: Override rank set to true, input data rank is " << input_data_rank << std::endl;
      return true;
    }catch(const std::exception &exc){
    }
  }
  return false;
#undef OPT
}

void printHelpMain(){
#define HELP(CMD,STR) std::cout << "-" << #CMD << ": " << STR << std::endl;
  HELP(parser_beginstep_timeout, "Set the timeout in seconds on waiting for the next ADIOS2 timestep (default 30s)");
  HELP(func_threshold_file, "Provide the path to a file containing HBOS/COPOD algorithm threshold overrides for specified functions. Format is JSON: \"[ { \"fname\": <FUNC>, \"threshold\": <THRES> },... ]\". Empty string (default) uses default threshold for all funcs");
  HELP(ignored_func_file, "Provide the path to a file containing function names (one per line) which the AD algorithm will ignore. All such events are labeled as normal. Empty string (default) performs AD on all events.");    
  HELP(anom_win_size, "When anomaly data are recorded a window of this size (in units of function execution events) around the anomalous event are also recorded (default 10)");
  HELP(trace_connect_timeout, "(For SST mode) Set the timeout in seconds on the connection to the TAU-instrumented binary (default 60s)");
  HELP(override_rank, "Set Chimbuko to overwrite the rank index in the parsed data with its own internal rank parameter. The value provided should be the original rank index of the data. This disables verification of the data rank.");
  HELP(outlier_statistic, "Set the statistic used for outlier detection. Options: exclusive_runtime (default), inclusive_runtime");
  HELP(prov_min_anom_time, "Set the minimum exclusive runtime (in microseconds) for anomalies to recorded in the provenance output (default 0)");
  HELP(monitoring_watchlist_file, "Provide a filename containing the counter watchlist for the integration with the monitoring plugin. Empty string (default) uses the default subset. File format is JSON: \"[ [<COUNTER NAME>, <FIELD NAME>], ... ]\" where COUNTER NAME is the name of the counter in the input data stream and FIELD NAME the name of the counter in the provenance output.");
  HELP(monitoring_counter_prefix, "Provide an optional prefix marking a set of monitoring plugin counters to be captured, on top of or superseding the watchlist. Empty string (default) is ignored.");
#undef HELP
}
 
void printHelp(){
  std::cout << "Usage: driver <Trace engine type> <Trace directory> <Trace file prefix> <Options>\n"
	    << "Where <Trace engine type> : BPFile or SST\n"
	    << "      <Trace directory>   : The directory in which the BPFile or SST file is located\n"
	    << "      <Trace file prefix> : The prefix of the file (the trace file name without extension e.g. \"tau-metrics-mybinary\" for \"tau-metrics-mybinary.bp\")\n"
	    << "      <Options>           : Optional arguments as described below.\n";
  printHelpBase();
  printHelpMain();
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

  //The remainder are optional arguments. Enable using the appropriate command line switch
  params.base_params.ad_algorithm = "hbos";
  params.base_params.hbos_threshold = 0.99;
  params.base_params.hbos_use_global_threshold = true;
  params.base_params.hbos_max_bins = 200;
  params.base_params.net_recv_timeout = 30000;
  params.base_params.pserver_addr = "";  //don't use pserver by default
  params.base_params.hpserver_nthr = 1;
  params.base_params.outlier_sigma = 6.0;     // anomaly detection algorithm parameter
  params.anom_win_size = 10; // size of window of events captured around anomaly
  params.base_params.perf_outputpath = ""; //don't use perf output by default
  params.base_params.perf_step = 10;   // make output every 10 steps
  params.base_params.prov_outputpath = "";
#ifdef ENABLE_PROVDB
  params.base_params.nprovdb_shards = 1;
  params.base_params.nprovdb_instances = 1;
  params.base_params.provdb_addr_dir = ""; //don't use provDB by default
#endif
  params.base_params.err_outputpath = ""; //use std::cerr for errors by default
  params.trace_connect_timeout = 60;
  params.parser_beginstep_timeout = 30;
  params.base_params.rank = -1234; //assign an invalid value as default for use below
  params.outlier_statistic = "exclusive_runtime";
  params.base_params.step_report_freq = 1;

  //getOptionalArgsParser().parse(params, argc-4, (const char**)(argv+4));

  //getOptionalBaseArgsParser().parse(params, argc-4, (const char**)(argv+4));
  int input_data_rank;

  char** argv_p = argv+4;
  int arg = 0;
  while(arg < argc-4){
    if(baseOptArgs(params,argv_p) || mainOptArgs(params,input_data_rank,argv_p)){
      arg += 2;
      argv_p += 2;
    }else{
      fatal_error("Unknown argument: "+std::string(*argv_p));
    }
  }

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
