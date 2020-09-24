//A fake AD that sends data to the provenance DB at a regular cadence
#define _PERF_METRIC
#include<mpi.h>
#include<chimbuko/ad/ADNetClient.hpp>
#include<chimbuko/ad/utils.hpp>
#include<chimbuko/util/commandLineParser.hpp>
#include<chimbuko/param/sstd_param.hpp>
#include<chimbuko/ad/ADEvent.hpp>
#include<chimbuko/util/Anomalies.hpp>
#include<chimbuko/ad/ADLocalFuncStatistics.hpp>
#include<chimbuko/ad/ADLocalCounterStatistics.hpp>
#include<chimbuko/ad/ADProvenanceDBclient.hpp>
#include<chimbuko/ad/ADMetadataParser.hpp>
#include<chimbuko/verbose.hpp>
#include "gtest/gtest.h"
#include<unit_test_common.hpp>

using namespace chimbuko;

struct Args{
  std::string provdb_addr;
  int cycles;
  int callstack_size;
  int ncounters;
  int winsize;
  int comm_messages_per_winevent;
  int anomalies_per_cycle;
  int normal_events_per_cycle;
  size_t cycle_time_ms;

  Args(){
    cycles = 10;
    callstack_size = 6;
    ncounters = 10;
    winsize=10;
    comm_messages_per_winevent = 2;
    anomalies_per_cycle = 10;
    normal_events_per_cycle = 100; //capture max 1 event per function per cycle (this should be changed!)
    cycle_time_ms = 1000;
  }
};

int main(int argc, char **argv){
  if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE")){
    std::cout << "Enabling verbose debug output" << std::endl;
    Verbose::set_verbose(true);
  }       

  MPI_Init(&argc, &argv);
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  commandLineParser<Args> cmdline;
  addMandatoryCommandLineArgDefaultHelpString(cmdline, provdb_addr);
  addOptionalCommandLineArgDefaultHelpString(cmdline, cycles);
  addOptionalCommandLineArgDefaultHelpString(cmdline, callstack_size);
  addOptionalCommandLineArgDefaultHelpString(cmdline, ncounters);
  addOptionalCommandLineArgDefaultHelpString(cmdline, winsize);
  addOptionalCommandLineArgDefaultHelpString(cmdline, comm_messages_per_winevent);
  addOptionalCommandLineArgDefaultHelpString(cmdline, anomalies_per_cycle);
  addOptionalCommandLineArgDefaultHelpString(cmdline, normal_events_per_cycle);
  addOptionalCommandLineArgDefaultHelpString(cmdline, cycle_time_ms);

  if(argc == 1 || (argc == 2 && std::string(argv[1]) == "-help")){
    cmdline.help();
    MPI_Finalize();
    return 0;
  }

  Args args;
  cmdline.parse(args, argc-1, (const char**)(argv+1));

  ADProvenanceDBclient provdb_client(rank);
  provdb_client.connect(args.provdb_addr);
  
  RunStats runstats; //any stats object
  for(int i=0;i<100;i++) runstats.push(i);
  nlohmann::json stats_j = runstats.get_json();

  //Call stack
  nlohmann::json callstack_entry = { {"fid",1}, {"func","test_function"}, {"entry",1234}, {"exit", 5678}, {"event_id", 1992}, {"is_anomaly", true} };
  std::vector<nlohmann::json> callstack(args.callstack_size, callstack_entry);
  
  //Counters
  nlohmann::json counter_entry = createCounterData_t(0,1,2,3,4,5,"counter").get_json();
  std::vector<nlohmann::json> counters(args.ncounters, counter_entry);

  //Exec and comm window
  nlohmann::json exec_window = {};
  exec_window["exec_window"] = nlohmann::json::array();
  exec_window["comm_window"] = nlohmann::json::array();

  nlohmann::json exec_entry = { {"fid", 0}, {"func", "function" }, {"event_id", 0}, {"entry", 1234}, {"exit", 5678}, {"parent_event_id", 999}, {"is_anomaly", true} };
  nlohmann::json comm_entry = createCommData_t(0,1,2,3,4,5,6,7,"SEND").get_json();
  for(int i=0;i<2*args.winsize;i++){
    exec_window["exec_window"].push_back(exec_entry);
    for(int j=0;j<args.comm_messages_per_winevent;j++)
      exec_window["comm_window"].push_back(comm_entry);
  }

  //GPU location and parent info
  nlohmann::json gpu_loc = GPUvirtualThreadInfo(0,0,0,0).get_json();
  nlohmann::json gpu_event_parent_stack = nlohmann::json::array();
  for(int i=0;i<args.callstack_size;i++) 
    gpu_event_parent_stack.push_back(callstack_entry);
  nlohmann::json gpu_parent = { {"event_id", "12:24:25"}, {"tid", 0} };
  gpu_parent["call_stack"] = gpu_event_parent_stack;


  //Put it all together
  nlohmann::json provenance_entry = {
      {"pid", 0},
	{"rid", 0},
	  {"tid", 0},
	    {"event_id", "0:12:250"},
	      {"fid", 0},
		{"func", "function"},
		  {"entry", 1234},
		    {"exit", 5678},
		      {"runtime_total", 2458},
			{"runtime_exclusive", 1922},
			  {"call_stack", callstack},
			    {"func_stats", stats_j},
			      {"counter_events", counters},
				{"is_gpu_event", true},
				  {"gpu_location", gpu_loc },
				    {"gpu_parent", gpu_parent},
				      {"event_window", exec_window},
					{"io_step", 199},
					  {"io_step_tstart", 0},
					    {"io_step_tend", 5000}
  };
  
  std::vector<nlohmann::json> anomaly_prov(args.anomalies_per_cycle,  provenance_entry);
  std::vector<nlohmann::json> normalevent_prov(args.normal_events_per_cycle,  provenance_entry);
 
  PerfStats stats(".","client_stats.json");
  provdb_client.linkPerf(&stats);

  PerfTimer cyc_timer, timer;
  for(int c=0;c<args.cycles;c++){
    cyc_timer.start();
    std::cout << "Rank " << rank << " starting cycle " << c << std::endl;

    //In the real client we use asynchronous sends but for the benchmark it is better to use blocking
    timer.start();
    provdb_client.sendMultipleData(anomaly_prov, ProvenanceDataType::AnomalyData);
    stats.add("anomaly_prov_send_ms", timer.elapsed_ms());

    timer.start();
    provdb_client.sendMultipleData(normalevent_prov, ProvenanceDataType::AnomalyData);
    stats.add("normalevent_prov_send_ms", timer.elapsed_ms());
    
    timer.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(args.cycle_time_ms));
    stats.add("sleep_ms", timer.elapsed_ms());

    stats.add("benchmark_cycle_time_ms", cyc_timer.elapsed_ms());
  }//cycle loop
  
  if(rank==0){
    stats.write();
  }

  MPI_Finalize();
  return 0;
}
