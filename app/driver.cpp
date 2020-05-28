// #include "chimbuko/AD.hpp"
#include "chimbuko/chimbuko.hpp"
#include "chimbuko/verbose.hpp"
#include <chrono>
#include <cstdlib>


using namespace chimbuko;
using namespace std::chrono;

template<typename T>
T strToAny(const std::string &s){
  T out;
  std::stringstream ss; ss << s; ss >> out;
  if(ss.fail()) throw std::runtime_error("Failed to parse \"" + s + "\"");
  return out;
}
template<>
inline std::string strToAny<std::string>(const std::string &s){ return s; }


ChimbukoParams getParamsFromCommandLine(int argc, char** argv, const int world_rank){
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


#define PARSE(TYPE, ARG) if(sarg == "-"#ARG){ params. ARG = strToAny<TYPE>(argv[arg+1]); arg+=2; } 

  int arg = 5;
  while(arg < argc){
    std::string sarg = argv[arg];

    PARSE(double, outlier_sigma)
    else PARSE(std::string, pserver_addr)
      else PARSE(int, viz_anom_winSize)
	else PARSE(int, interval_msec)
#ifdef ENABLE_PROVDB
	  else PARSE(std::string, provdb_addr)
#endif
#ifdef _PERF_METRIC
	  else PARSE(std::string, perf_outputpath)
	    else PARSE(int, perf_step)
#endif
	      else throw std::runtime_error("Unrecognized argument: " + sarg);
  }

  return params;
}




int main(int argc, char ** argv)
{
  MPI_Init(&argc, &argv);

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
  return 0;
}
