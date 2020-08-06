// #include "chimbuko/AD.hpp"
#include "chimbuko/chimbuko.hpp"
#include "chimbuko/verbose.hpp"
#include "chimbuko/util/string.hpp"
#include "chimbuko/util/commandLineParser.hpp"
#include <chrono>
#include <cstdlib>


using namespace chimbuko;
using namespace std::chrono;

typedef commandLineParser<ChimbukoParams> optionalArgsParser;

optionalArgsParser & getOptionalArgsParser(){
  static bool initialized = false;
  static optionalArgsParser p;
  if(!initialized){
    addOptionalCommandLineArg(p, outlier_sigma, "Set the number of standard deviations that defines an anomalous event (default 6)");
    addOptionalCommandLineArg(p, pserver_addr, "Set the address of the parameter server. If empty (default) the pserver will not be used.");
    addOptionalCommandLineArg(p, viz_anom_winSize, "When anomaly data are written, a window of this size (in units of events) around the anomalous event are also sent (default 0)");
    addOptionalCommandLineArg(p, interval_msec, "Force the AD to pause for this number of ms at the end of each IO step (default 0)");
#ifdef ENABLE_PROVDB
    addOptionalCommandLineArg(p, provdb_addr, "Address of the provenance database. If empty (default) the provenance DB will not be used.\nHas format \"ofi+tcp;ofi_rxm://${IP_ADDR}:${PORT}\". Should also accept \"tcp://${IP_ADDR}:${PORT}\"");    
#endif    
#ifdef _PERF_METRIC
    addOptionalCommandLineArg(p, perf_outputpath, "Output path for AD performance monitoring data. If an empty string (default) no output is written.");
    addOptionalCommandLineArg(p, perf_step, "How frequently (in IO steps) the performance data is dumped (default 10)");
#endif
    initialized = true;
  }
  return p;
};

void printHelp(){
  std::cout << "Usage: driver <Trace engine type> <Trace directory> <Trace file prefix> <Output location> <Options>\n" 
	    << "Where <Trace engine type> : BPFile or SST\n"
	    << "      <Trace directory>   : The directory in which the BPFile or SST file is located\n"
	    << "      <Trace file prefix> : The prefix of the file eg \"tau-metrics-[mybinary]\"\n"
	    << "      <Output location>   : Where the data intended for the visualization module is sent.\n"
	    << "                            If a url (starts with http) it is sent to the url,\n"
	    << "                            otherwise if a non-empty string it is output to disk at that location\n"
	    << "      <Options>           : Optional arguments as described below.\n";
  getOptionalArgsParser().help(std::cout);
}

ChimbukoParams getParamsFromCommandLine(int argc, char** argv, const int world_rank){
  if(argc < 5){
    std::cerr << "Expected at least 5 arguments: <exe> <BPFile/SST> <.bp location> <bp file prefix> <output (ip/dir)>" << std::endl;
    exit(-1);
  }

  // -----------------------------------------------------------------------
  // Parse command line arguments (cf chimbuko.hpp for detailed description of parameters)
  // -----------------------------------------------------------------------
  ChimbukoParams params;
  params.verbose = world_rank == 0; //head node produces verbose output
  params.rank = world_rank;
      
  //Parameters for the connection to the instrumented binary trace output
  params.trace_engineType = argv[1]; // BPFile or SST
  params.trace_data_dir = argv[2]; // *.bp location
  std::string bp_prefix = argv[3]; // bp file prefix (e.g. tau-metrics-[nwchem])
  params.trace_inputFile = bp_prefix + "-" + std::to_string(world_rank) + ".bp";

  //Choose how the data intended for the vizualization module is handled
  // if string starts with "http", use online mode (connect to viz module); otherwise offline mode (dump to disk)
  std::string output = argv[4]; 
  params.viz_iomode = IOMode::Both;
  params.viz_datadump_outputPath = "";      //output directory
  params.viz_addr = "";
  if (output.find("http://") == std::string::npos)
    params.viz_datadump_outputPath = output;
  else
    params.viz_addr = output;

  //The remainder are optional arguments. Enable using the appropriate command line switch

  params.pserver_addr = "";  //address of parameter server
  params.outlier_sigma = 6.0;     // anomaly detection algorithm parameter
  params.viz_anom_winSize = 0; // size of window of events captured around anomaly
  params.interval_msec = 0; //pause at end of each io step
  params.perf_outputpath = ""; // performance output path
  params.perf_step = 10;   // make output every 10 steps

  getOptionalArgsParser().parse(params, argc-5, (const char**)(argv+5));

  return params;
}
  
 


int main(int argc, char ** argv){
  if(argc == 1 || (argc == 2 && std::string(argv[1]) == "-help") ){
    printHelp();
    return 0;
  }
      
  assert( MPI_Init(&argc, &argv) == MPI_SUCCESS );

  int world_rank, world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);    

  try 
    {

      //Parse environment variables
      if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE")){
	std::cout << "Enabling verbose debug output" << std::endl;
	Verbose::set_verbose(true);
      }       

      //Parse Chimbuko parameters
      ChimbukoParams params = getParamsFromCommandLine(argc, argv, world_rank);
      if(world_rank == 0) params.print();

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
      if (world_rank == 0) {
	std::cout << "rank: " << world_rank 
		  << " analysis start " << (driver.use_ps() ? "with": "without") 
		  << " pserver" << std::endl;
      }

      t1 = high_resolution_clock::now();
      driver.run(n_func_events, 
		 n_comm_events,
		 n_counter_events,
		 n_outliers,
		 frames);
      t2 = high_resolution_clock::now();
        
      if (world_rank == 0) {
	std::cout << "rank: " << world_rank << " analysis done!\n";
	driver.show_status(true);
      }

      // -----------------------------------------------------------------------
      // Average analysis time and total number of outliers
      // -----------------------------------------------------------------------
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

        
      if (world_rank == 0) {
	std::cout << "\n"
		  << "Avg. num. frames     : " << (double)total_frames/(double)world_size << "\n"
		  << "Avg. processing time : " << (double)total_processing_time/(double)world_size << " msec\n"
		  << "Total num. outliers  : " << total_n_outliers << "\n"
		  << "Total func events    : " << total_n_func_events << "\n"
		  << "Total comm events    : " << total_n_comm_events << "\n"
		  << "Total counter events    : " << total_n_counter_events << "\n"
		  << "Total function/comm events         : " << total_n_func_events + total_n_comm_events
		  << std::endl;
      }
    }
  catch (std::invalid_argument &e)
    {
      std::cout << e.what() << std::endl;
      //todo: usages()
    }
  catch (std::ios_base::failure &e)
    {
      std::cout << "I/O base exception caught\n";
      std::cout << e.what() << std::endl;
    }
  catch (std::exception &e)
    {
      std::cout << "Exception caught\n";
      std::cout << e.what() << std::endl;
    }

  MPI_Finalize();
  VERBOSE(std::cout << "Rank " << world_rank << ": driver is exiting" << std::endl);
  return 0;
}
