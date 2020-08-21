//A fake AD that sends data to the pserver at a regular cadence
#include<mpi.h>
#include<chimbuko/ad/ADNetClient.hpp>
#include<chimbuko/util/commandLineParser.hpp>
#include<chimbuko/param/sstd_param.hpp>
#include<chimbuko/ad/ADEvent.hpp>
#include<chimbuko/util/Anomalies.hpp>
#include<chimbuko/ad/ADLocalFuncStatistics.hpp>
#include<chimbuko/ad/ADLocalCounterStatistics.hpp>
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
  
  Args(){
    cycles = 10;
    nfuncs = 100;
    ncounters = 100;
    nanomalies_per_func = 2;
    cycle_time_ms = 1000;
  }
};

int main(int argc, char **argv){
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

  if(argc == 1 || (argc == 2 && std::string(argv[1]) == "-help")){
    cmdline.help();
    MPI_Finalize();
    return 0;
  }

  Args args;
  cmdline.parse(args, argc-1, (const char**)(argv+1));

  ADNetClient net_client;
  net_client.connect_ps(rank, 0, args.pserver_addr);

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


  for(int c=0;c<args.cycles;c++){
    std::cout << "Rank " << rank << " starting cycle " << c << std::endl;
    //Send a parameters object
    {
      Message msg;
      msg.set_info(rank, 0, MessageType::REQ_ADD, MessageKind::PARAMETERS);
      msg.set_msg(params.serialize(), false);
      net_client.send_and_receive(msg);
    }

   
    //Send fake function and counter statistics
    {    
      ADLocalFuncStatistics prof_stats(c);
      prof_stats.gatherStatistics(&fake_exec_map);
      prof_stats.gatherAnomalies(anomalies);
      prof_stats.updateGlobalStatistics(net_client);
      
      ADLocalCounterStatistics count_stats(c, nullptr); //currently collect all counters
      for(auto const &e : counter_stats)
	count_stats.setStats(e.first,e.second);
      count_stats.updateGlobalStatistics(net_client);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(args.cycle_time_ms));
  }//cycle loop
 
  }
  MPI_Finalize();
  return 0;
}
