//A fake AD that sends data to the pserver at a regular cadence
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
#include<chimbuko/verbose.hpp>
#include "gtest/gtest.h"
#include<unit_test_common.hpp>

using namespace chimbuko;

struct Args{
  std::string pserver_addr;
  int cycles;
  int nfuncs;
  int ncounters;
  int nanomalies_per_func;
  size_t cycle_time_ms;
  int hpserver_nthr;

  Args(){
    cycles = 10;
    nfuncs = 100;
    ncounters = 100;
    nanomalies_per_func = 2;
    cycle_time_ms = 1000;
    hpserver_nthr = 1;
  }
};

int main(int argc, char **argv){
  if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE")){
    std::cout << "Enabling verbose debug output" << std::endl;
    Verbose::set_verbose(true);
  }       

  MPI_Init(&argc, &argv);
  {

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  commandLineParser<Args> cmdline;
  addMandatoryCommandLineArgDefaultHelpString(cmdline, pserver_addr);
  addOptionalCommandLineArgDefaultHelpString(cmdline, cycles);
  addOptionalCommandLineArgDefaultHelpString(cmdline, nfuncs);
  addOptionalCommandLineArgDefaultHelpString(cmdline, ncounters);
  addOptionalCommandLineArgDefaultHelpString(cmdline, nanomalies_per_func);
  addOptionalCommandLineArgDefaultHelpString(cmdline, cycle_time_ms);
  addOptionalCommandLineArgDefaultHelpString(cmdline, hpserver_nthr);


  if(argc == 1 || (argc == 2 && std::string(argv[1]) == "-help")){
    cmdline.help();
    MPI_Finalize();
    return 0;
  }

  Args args;
  cmdline.parse(args, argc-1, (const char**)(argv+1));

  if(args.hpserver_nthr > 1){
    std::string orig = args.pserver_addr;
    args.pserver_addr = getHPserverIP(args.pserver_addr, args.hpserver_nthr, rank);
    std::cout << "Client rank " << rank << " connecting to endpoint " << args.pserver_addr << " (base " << orig << ")" << std::endl;
  }

  PerfStats stats;

  ADNetClient net_client;
  net_client.connect_ps(rank, 0, args.pserver_addr);
  net_client.linkPerf(&stats);  

  //Set up a params object with the required number of params
  SstdParam params;
  for(int i=0;i<args.nfuncs;i++){
    RunStats &r = params[i];
    for(int j=0;j<100;j++)
      r.push(double(j));
  }

  int nevent = 100;
  if(nevent < args.nanomalies_per_func) nevent = args.nanomalies_per_func;
  
  //Create the fake function statistics and anomalies
  std::cout << "Rank " << rank << " generating fake function stats and anomalies" << std::endl;
  CallList_t fake_execs;
  ExecDataMap_t fake_exec_map;
  Anomalies anomalies;

  for(int i=0;i<args.nfuncs;i++){
    for(int j=0;j<nevent;j++){
      ExecData_t e = createFuncExecData_t(0, rank, 0,
					  i, "func"+anyToStr(i),
					  100*i, 100);
      auto it = fake_execs.insert(fake_execs.end(),e);
      fake_exec_map[i].push_back(it);

      if(j<args.nanomalies_per_func){
	anomalies.insert(it, Anomalies::EventType::Outlier);
      }

    }
  }

  //Create the fake counters
  std::cout << "Rank " << rank << " generating fake counters" << std::endl;
  std::unordered_map<std::string, RunStats> counter_stats;
  for(int i=0;i<args.ncounters;i++){
    RunStats stats;
    for(int j=0;j<100;j++)
      stats.push(double(j));
    counter_stats["counter"+anyToStr(i)] = stats;
  }

  PerfTimer cyc_timer;
  PerfTimer timer;

  //To make the benchmark as lightweight as possible, precompute the messages and send the same each cycle
  std::string params_msg = params.serialize();

  ADLocalFuncStatistics prof_stats(0);
  prof_stats.gatherStatistics(&fake_exec_map);
  prof_stats.gatherAnomalies(anomalies);
  std::string prof_stats_msg = prof_stats.get_state(net_client.get_client_rank()).serialize_cerealpb();

  ADLocalCounterStatistics count_stats(0, nullptr); //currently collect all counters
  for(auto const &e : counter_stats)
    count_stats.setStats(e.first,e.second);
  std::string count_stats_msg = count_stats.get_state().serialize_cerealpb();


  for(int c=0;c<args.cycles;c++){
    cyc_timer.start();
    std::cout << "Rank " << rank << " starting cycle " << c << std::endl;
    //Send a parameters object
    {
      timer.start();
      Message msg;
      msg.set_info(rank, 0, MessageType::REQ_ADD, MessageKind::PARAMETERS);
      msg.set_msg(params_msg, false);
      stats.add("param_update_msg_format_ms", timer.elapsed_ms());

      timer.start();
      net_client.send_and_receive(msg);
      stats.add("param_update_comms_ms", timer.elapsed_ms());
    }

    //Send fake function and counter statistics
    {
      timer.start();
      Message msg;
      msg.set_info(net_client.get_client_rank(), net_client.get_server_rank(), MessageType::REQ_ADD, MessageKind::ANOMALY_STATS, c);
      msg.set_msg(prof_stats_msg);
      stats.add("funcstats_msg_format_ms", timer.elapsed_ms());
     
      timer.start();
      std::string strmsg = net_client.send_and_receive(msg);
      stats.add("funcstats_update_comms_ms", timer.elapsed_ms());
    }
    {
      timer.start();
      Message msg;
      msg.set_info(net_client.get_client_rank(), net_client.get_server_rank(), MessageType::REQ_ADD, MessageKind::COUNTER_STATS, c);
      msg.set_msg(count_stats_msg);
      stats.add("countstats_msg_format_ms", timer.elapsed_ms());
     
      timer.start();
      std::string strmsg = net_client.send_and_receive(msg);
      stats.add("countstats_update_comms_ms", timer.elapsed_ms());
    }

    timer.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(args.cycle_time_ms));
    stats.add("sleep_ms", timer.elapsed_ms());

    stats.add("benchmark_cycle_time_ms", cyc_timer.elapsed_ms());
  }//cycle loop
  
  if(rank==0){
    stats.setWriteLocation(".","client_stats.json");
    stats.write();
  }

  }
  MPI_Finalize();
  return 0;
}
