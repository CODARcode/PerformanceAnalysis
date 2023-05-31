//A fake AD that sends data to the pserver at a regular cadence
#include<mpi.h>
#include<chimbuko/ad/ADNetClient.hpp>
#include<chimbuko/ad/utils.hpp>
#include<chimbuko/util/commandLineParser.hpp>
#include<chimbuko/param/sstd_param.hpp>
#include<chimbuko/param/hbos_param.hpp>
#include<chimbuko/param/copod_param.hpp>
#include<chimbuko/ad/ADEvent.hpp>
#include<chimbuko/util/Anomalies.hpp>
#include<chimbuko/ad/ADLocalFuncStatistics.hpp>
#include<chimbuko/ad/ADLocalCounterStatistics.hpp>
#include<chimbuko/ad/ADLocalAnomalyMetrics.hpp>
#include<chimbuko/ad/ADcombinedPSdata.hpp>
#include<chimbuko/ad/ADglobalFunctionIndexMap.hpp>
#include<chimbuko/verbose.hpp>
#include "gtest/gtest.h"
#include<unit_test_common.hpp>

using namespace chimbuko;

struct Args{
  int nfuncs;
  std::string algorithm;
  int hbos_bins;
  int cycles;
  
  Args(){
    nfuncs = 100;
    algorithm = "hbos";
    hbos_bins = 20;
    cycles = 10000;
  }
};

int main(int argc, char **argv){
  MPI_Init(&argc, &argv);
  
  if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE")){
    std::cout << "Enabling verbose debug output" << std::endl;
    enableVerboseLogging() = true;
  }       

  commandLineParser<Args> cmdline;
  addOptionalCommandLineArgDefaultHelpString(cmdline, cycles);
  addOptionalCommandLineArgDefaultHelpString(cmdline, nfuncs);
  addOptionalCommandLineArgDefaultHelpString(cmdline, algorithm); //algorithm, default "sstd"
  addOptionalCommandLineArgDefaultHelpString(cmdline, hbos_bins); //default 20

  if(argc == 1 || (argc == 2 && std::string(argv[1]) == "-help")){
    cmdline.help();
    MPI_Finalize();
    return 0;
  }

  Args args;
  cmdline.parse(args, argc-1, (const char**)(argv+1));

  //Set up a params object with the required number of params
  ParamInterface *params;
  if(args.algorithm == "sstd"){
    SstdParam *p = new SstdParam;
    for(int i=0;i<args.nfuncs;i++){
      RunStats &r = (*p)[i];
      for(int j=0;j<100;j++)
	r.push(double(j));
    }
    params = p;
  }else if(args.algorithm == "hbos" || args.algorithm == "copod"){
    std::vector<unsigned int> counts(args.hbos_bins);
    for(int i=0;i<args.hbos_bins;i++) counts[i] = i;
    Histogram hd;
    hd.set_histogram(counts, 0.0001, args.hbos_bins, 0, 1);

    if(args.algorithm == "hbos"){
      HbosParam *p = new HbosParam;
      for(int i=0;i<args.nfuncs;i++) (*p)[i].getHistogram() = hd;
      params = p;
    }else{
      CopodParam *p = new CopodParam;
      for(int i=0;i<args.nfuncs;i++) (*p)[i].getHistogram() = hd;
      params = p;
    }
  }else{
    fatal_error("Unknown AD algorithm");
  }

  PerfTimer cyc_timer;
  PerfTimer timer;

  //To make the benchmark as lightweight as possible, precompute the messages and send the same each cycle
  std::string params_msg = params->serialize(); 
  RunStats r;  

  for(int c=0;c<args.cycles;c++){
    cyc_timer.start();
    std::cout << c << std::endl;
      
    timer.start();
    params->update(params_msg, false);
    r.push(timer.elapsed_ms());    
  }//cycle loop
  std::cout << r.mean() << " " << r.stddev() << std::endl;

  delete params;
  MPI_Finalize();
  
  return 0;
}
