//The hierarchical parameter server main program. This program collects statistics from the node-instances of the anomaly detector
#include <chimbuko_config.h>
#ifndef _USE_ZMQNET
#include<iostream>
#warning "Hierarchical parameter server requires ZMQNet"

int main(void){
  std::cerr << "Hierarchical parameter server requires ZMQNet" << std::endl;
  return 1;
}

#else

#include <chimbuko/pserver.hpp>
#include <chimbuko/net/zmqme_net.hpp>
#ifdef USE_MPI
#include <mpi.h>
#endif
#include <chimbuko/param/sstd_param.hpp>
#include <chimbuko/util/commandLineParser.hpp>
#include <chimbuko/verbose.hpp>
#include <fstream>

using namespace chimbuko;

struct hpserverArgs{
  int nt;
  std::string logdir;
  std::string ws_addr;

  std::string load_params;
  bool load_params_set;

  std::string save_params;
  bool save_params_set;

  bool freeze_params;

  int stat_send_freq;

  std::string stat_outputdir;

  int base_port;

  hpserverArgs(): nt(-1), logdir("."), ws_addr(""), load_params_set(false), save_params_set(false), freeze_params(false), stat_send_freq(1000), stat_outputdir(""), base_port(5559)
  {}

  static commandLineParser<hpserverArgs> &getParser(){
    static bool init = false;
    static commandLineParser<hpserverArgs> p;
    if(!init){
      addOptionalCommandLineArg(p, nt, "Set the number of RPC handler threads (max-2 by default)");
      addOptionalCommandLineArg(p, logdir, "Set the output log directory (default: job directory)");
      addOptionalCommandLineArg(p, ws_addr, "Provide the address of the visualization module (aka webserver). If not provided no information will be sent to the visualization");
      addOptionalCommandLineArg(p, stat_outputdir, "Optionally provide a directory where the stat data will be written alongside/in place of sending to the viz module (default: unused");
      addOptionalCommandLineArgWithFlag(p, load_params, load_params_set, "Load previously computed anomaly algorithm parameters from file");
      addOptionalCommandLineArgWithFlag(p, save_params, save_params_set, "Save anomaly algorithm parameters to file");
      addOptionalCommandLineArg(p, freeze_params, "Fix the anomaly algorithm parameters, preventing updates from the AD. Use in conjunction with -load_params. Value should be 'true' or 'false' (or 0/1)");
      addOptionalCommandLineArg(p, stat_send_freq, "The frequency in ms at which statistics are sent to the visualization (default 1000ms)");
      addOptionalCommandLineArg(p, base_port, "The base port. Thread worker ports are base_port+thread_index (default 5559)");
      init = true;
    }
    return p;
  }
};


int main (int argc, char ** argv){
  std::cout << "WARNING: hpserver application is incomplete! Use pserver for production running." << std::endl;
  
  if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE")){
    std::cout << "HPServer enabling verbose debug output" << std::endl;
    enableVerboseLogging() = true;
  }       


  hpserverArgs args;
  if(argc < hpserverArgs::getParser().nMandatoryArgs()+1 || (argc == 2 && std::string(argv[1]) == "-help") ){
    hpserverArgs::getParser().help(std::cout);
    return argc < hpserverArgs::getParser().nMandatoryArgs()+1 ? 1 : 0;
  }
  hpserverArgs::getParser().parse(args, argc-1, (const char**)(argv+1));

  if (args.nt <= 0) {
    args.nt = std::max(
		       (int)std::thread::hardware_concurrency() - 5,
		       1
		       );
    std::cout << "HPserver set number of threads to " << args.nt << std::endl;     
  }

  //Each thread acts on its own instance of the data, avoiding locks
  //The exception is the global function index map which is used infrequently and must be kept synchronized
  std::vector<SstdParam> param(args.nt); //global collection of parameters used to identify anomalies
  std::vector<GlobalAnomalyStats> global_func_stats(args.nt); //global anomaly statistics
  std::vector<GlobalCounterStats> global_counter_stats(args.nt); //global counter statistics
  std::vector<GlobalAnomalyMetrics> global_anom_metrics(args.nt); //global anomaly metrics
  PSglobalFunctionIndexMap global_func_index_map; //mapping of function name to global index
  
  if(args.load_params_set){
    std::cout << "Loading parameters from input file " << args.load_params << std::endl;
    std::ifstream in(args.load_params);
    if(!in.good()) throw std::runtime_error("Could not load anomaly algorithm parameters from the file provided");
    nlohmann::json in_p;
    in >> in_p;
    global_func_index_map.deserialize(in_p["func_index_map"]);
    SstdParam tmp_param;   
    tmp_param.assign(in_p["alg_params"].dump());
    for(int t=0;t<args.nt;t++) param[t].assign(tmp_param.get_runstats());
  }
  
  ZMQMENet net;
  net.setBasePort(args.base_port);
#ifdef USE_MPI
  MPI_Init(&argc, &argv);
#endif

  PSstatSender stat_sender(args.stat_send_freq);

  try {
    std::cout << "Run hparameter server with " << args.nt << " threads" << std::endl;

    if (args.ws_addr.size() || args.stat_outputdir.size()){
      std::cout << "Run anomaly statistics sender ";
      if(args.ws_addr.size()) std::cout << "(ws @ " << args.ws_addr << ")";
      if(args.stat_outputdir.size()) std::cout << "(dir @ " << args.stat_outputdir << ")";
    }

    for(int t=0;t<args.nt;t++){
      net.add_payload(new NetPayloadUpdateParams(&param[t], args.freeze_params), t);
      net.add_payload(new NetPayloadGetParams(&param[t]), t);
      net.add_payload(new NetPayloadRecvCombinedADdata(&global_func_stats[t], &global_counter_stats[t], &global_anom_metrics[t]), t);
      net.add_payload(new NetPayloadGlobalFunctionIndexMapBatched(&global_func_index_map), t);
      net.add_payload(new NetPayloadPing,t);   
    }
    net.init(nullptr, nullptr, args.nt);

    //Start sending anomaly statistics to viz
    //Todo: synchronize between thread instances
    stat_sender.add_payload(new PSstatSenderGlobalAnomalyStatsPayload(&global_func_stats[0]));
    stat_sender.add_payload(new PSstatSenderGlobalCounterStatsPayload(&global_counter_stats[0]));
    stat_sender.add_payload(new PSstatSenderGlobalAnomalyMetricsPayload(&global_anom_metrics[0]));
    stat_sender.run_stat_sender(args.ws_addr, args.stat_outputdir);

    //Start communicating with the AD instances
#ifdef _PERF_METRIC
    net.run(args.logdir);
#else
    net.run();
#endif

    // at this point, all pseudo AD modules finished sending 
    // anomaly statistics data
    std::this_thread::sleep_for(std::chrono::seconds(1));
    stat_sender.stop_stat_sender(1000);

    // could be output to a file
    std::cout << "Shutdown parameter server ..." << std::endl;
    std::ofstream o;
    o.open(args.logdir + "/parameters.txt");
    if (o.is_open())
      {
	param[0].show(o);
	o.close();
      }
  }
  catch (std::invalid_argument &e)
    {
      std::cout << e.what() << std::endl;
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

  std::cout << "HPserver finalizing the network" << std::endl;
  net.finalize();
#if defined(_USE_ZMQNET) && defined(USE_MPI)
  MPI_Finalize();
#endif

  if(args.save_params_set){
    //Todo: final param sync to and write complete stats
    std::cout << "Saving parameters to output file " << args.save_params << std::endl;   
    std::ofstream out(args.save_params);
    if(!out.good()) throw std::runtime_error("Could not write anomaly algorithm parameters to the file provided");
    nlohmann::json out_p;
    out_p["func_index_map"] = global_func_index_map.serialize();
    out_p["alg_params"] = nlohmann::json::parse(param[0].serialize());
    out << out_p;
  }

  std::cout << "HPserver finished" << std::endl;
  return 0;
}



#endif //ZMQNET
