#include "chimbuko/chimbuko.hpp"
#include "chimbuko/verbose.hpp"
#include "chimbuko/util/string.hpp"
#include "chimbuko/util/commandLineParser.hpp"
#include <chrono>
#include <cstdlib>


using namespace chimbuko;
using namespace std::chrono;

typedef commandLineParser<ChimbukoParams> optionalArgsParser;

//Specialized class for setting the override_rank
struct overrideRankArg: public optionalCommandLineArgBase<ChimbukoParams>{
  static int & input_data_rank(){ static int v; return v; }

  int parse(ChimbukoParams &into, const std::string &arg, const char** vals, const int vals_size){
    if(arg == "-override_rank"){
      if(vals_size < 1) return -1;
      int v;
      try{
	v = strToAny<int>(vals[0]);
      }catch(const std::exception &exc){
	return -1;
      }
      input_data_rank() = v;
      into.override_rank = true;
      verboseStream << "Driver: Override rank set to true, input data rank is " << input_data_rank() << std::endl;
      return 1;
    }
    return -1;
  }
  void help(std::ostream &os) const{
    os << "-override_rank : Set Chimbuko to overwrite the rank index in the parsed data with its own internal rank parameter. The value provided should be the original rank index of the data. This disables verification of the data rank.";
  }
};


//Specialized class for setting the global head rank for logging purposes (doesn't actually set a parameter in the struct)
struct setLoggingHeadRankArg: public optionalCommandLineArgBase<ChimbukoParams>{
  int parse(ChimbukoParams &into, const std::string &arg, const char** vals, const int vals_size){
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
    addOptionalCommandLineArg(p, program_idx, "Set the index associated with the instrumented program. Use to label components of a workflow. (default 0)");
    addOptionalCommandLineArg(p, outlier_sigma, "Set the number of standard deviations that defines an anomalous event (default 6)");
    addOptionalCommandLineArg(p, pserver_addr, "Set the address of the parameter server. If empty (default) the pserver will not be used.");
    addOptionalCommandLineArg(p, hpserver_nthr, "Set the number of threads used by the hierarchical PS. This parameter is used to compute a port offset for the particular endpoint that this AD rank connects to (default 1)");
    addOptionalCommandLineArg(p, interval_msec, "Force the AD to pause for this number of ms at the end of each IO step (default 0)");
    addOptionalCommandLineArg(p, anom_win_size, "When anomaly data are recorded a window of this size (in units of function execution events) around the anomalous event are also recorded (default 10)");
    addOptionalCommandLineArg(p, prov_outputpath, "Output provenance data to this directory. Can be used in place of or in conjunction with the provenance database. An empty string \"\" (default) disables this output");
#ifdef ENABLE_PROVDB
    addOptionalCommandLineArg(p, provdb_addr, "Address of the provenance database. If empty (default) the provenance DB will not be used.\nHas format \"ofi+tcp;ofi_rxm://${IP_ADDR}:${PORT}\". Should also accept \"tcp://${IP_ADDR}:${PORT}\"");    
    addOptionalCommandLineArg(p, nprovdb_shards, "Number of provenance database shards. Clients connect to shards round-robin by rank (default 1)");    
#endif    
#ifdef _PERF_METRIC
    addOptionalCommandLineArg(p, perf_outputpath, "Output path for AD performance monitoring data. If an empty string (default) no output is written.");
    addOptionalCommandLineArg(p, perf_step, "How frequently (in IO steps) the performance data is dumped (default 10)");
#endif
    addOptionalCommandLineArg(p, err_outputpath, "Directory in which to place error logs. If an empty string (default) the errors will be piped to std::cerr");
    addOptionalCommandLineArg(p, trace_connect_timeout, "(For SST mode) Set the timeout in seconds on the connection to the TAU-instrumented binary (default 60s)");
    addOptionalCommandLineArg(p, parser_beginstep_timeout, "Set the timeout in seconds on waiting for the next ADIOS2 timestep (default 30s)");

    addOptionalCommandLineArg(p, rank, "Set the rank index of the trace data. Used for verification unless override_rank is set. A value < 0 signals the value to be equal to the MPI rank of Chimbuko driver (default)");
    p.addOptionalArg(new overrideRankArg); //-override_rank <idx>
    p.addOptionalArg(new setLoggingHeadRankArg); //-logging_head_rank <rank>

    addOptionalCommandLineArg(p, outlier_statistic, "Set the statistic used for outlier detection. Options: exclusive_runtime (default), inclusive_runtime");
    addOptionalCommandLineArg(p, step_report_freq, "Set the steps between Chimbuko reporting IO step progress. Use 0 to deactivate this logging entirely (default 1)");

    initialized = true;
  }
  return p;
};

void printHelp(){
  std::cout << "Usage: driver <Trace engine type> <Trace directory> <Trace file prefix> <Options>\n" 
	    << "Where <Trace engine type> : BPFile or SST\n"
	    << "      <Trace directory>   : The directory in which the BPFile or SST file is located\n"
	    << "      <Trace file prefix> : The prefix of the file (the trace file name without extension e.g. \"tau-metrics-mybinary\" for \"tau-metrics-mybinary.bp\")\n"
	    << "      <Options>           : Optional arguments as described below.\n";
  getOptionalArgsParser().help(std::cout);
}

ChimbukoParams getParamsFromCommandLine(int argc, char** argv, const int mpi_world_rank){
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

  params.pserver_addr = "";  //don't use pserver by default
  params.hpserver_nthr = 1;
  params.outlier_sigma = 6.0;     // anomaly detection algorithm parameter
  params.anom_win_size = 10; // size of window of events captured around anomaly
  params.perf_outputpath = ""; //don't use perf output by default
  params.perf_step = 10;   // make output every 10 steps
  params.prov_outputpath = "";
#ifdef ENABLE_PROVDB
  params.nprovdb_shards = 1;
  params.provdb_addr = ""; //don't use provDB by default
#endif
  params.err_outputpath = ""; //use std::cerr for errors by default
  params.trace_connect_timeout = 60;
  params.parser_beginstep_timeout = 30;
  params.rank = -1234; //assign an invalid value as default for use below
  params.outlier_statistic = "exclusive_runtime";
  params.step_report_freq = 1;
  
  getOptionalArgsParser().parse(params, argc-4, (const char**)(argv+4));

  //By default assign the rank index of the trace data as the MPI rank of the AD process
  //Allow override by user
  if(params.rank < 0)
    params.rank = mpi_world_rank;

  params.verbose = params.rank == 0; //head node produces verbose output

  //Assume the rank index of the data is the same as the driver rank parameter
  params.trace_inputFile = bp_prefix + "-" + std::to_string(params.rank) + ".bp";

  //If we are forcing the parsed data rank to match the driver rank parameter, this implies it was not originally
  //Thus we need to obtain the input data rank also from the command line and modify the filename accordingly
  if(params.override_rank)
    params.trace_inputFile = bp_prefix + "-" + std::to_string(overrideRankArg::input_data_rank()) + ".bp";
  
  //If neither the provenance database or the provenance output path are set, default to outputting to pwd
  if(params.prov_outputpath.size() == 0
#ifdef ENABLE_PROVDB
     && params.provdb_addr.size() == 0
#endif
     ){
    params.prov_outputpath = ".";
  }

  return params;
}
  
 


int main(int argc, char ** argv){
  if(argc == 1 || (argc == 2 && std::string(argv[1]) == "-help") ){
    printHelp();
    return 0;
  }
      
  assert( MPI_Init(&argc, &argv) == MPI_SUCCESS );

  int mpi_world_rank, mpi_world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_world_size);    

  //Parse environment variables
  if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE"))
    enableVerboseLogging() = true;

  //Parse Chimbuko parameters
  ChimbukoParams params = getParamsFromCommandLine(argc, argv, mpi_world_rank);
  if(params.rank == progressHeadRank()) params.print();

  if(enableVerboseLogging())
    headProgressStream(params.rank) << "Driver rank " << params.rank << ": Enabling verbose debug output" << std::endl;


  verboseStream << "Driver rank " << params.rank << ": waiting at pre-run barrier" << std::endl;
  MPI_Barrier(MPI_COMM_WORLD);

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
      headProgressStream(params.rank) << "Driver rank " << params.rank 
				  << ": analysis start " << (driver.use_ps() ? "with": "without") 
				  << " pserver" << std::endl;

      t1 = high_resolution_clock::now();
      driver.run(n_func_events, 
		 n_comm_events,
		 n_counter_events,
		 n_outliers,
		 frames);
      t2 = high_resolution_clock::now();
        
      if (params.rank == 0) {
	headProgressStream(params.rank) << "Driver rank " << params.rank << ": analysis done!\n";
	driver.show_status(true);
      }

      // -----------------------------------------------------------------------
      // Average analysis time and total number of outliers
      // -----------------------------------------------------------------------
      verboseStream << "Driver rank " << params.rank << ": waiting at post-run barrier" << std::endl;
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
        
      headProgressStream(params.rank) << "Driver rank " << params.rank << ": Final report\n"
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
    std::cout << '[' << getDateTime() << ", rank " << params.rank << "] Driver : caught invalid argument: " << e.what() << std::endl;
    error = true;
  }
  catch (const std::ios_base::failure &e){
    std::cout << '[' << getDateTime() << ", rank " << params.rank << "] Driver : I/O base exception caught: " << e.what() << std::endl;
    error = true;
  }
  catch (const std::exception &e){
    std::cout << '[' << getDateTime() << ", rank " << params.rank << "] Driver : Exception caught: " << e.what() << std::endl;
    error = true;
  }

  MPI_Finalize();
  headProgressStream(params.rank) << "Driver is exiting" << std::endl;
  return error ? 1 : 0;
}
