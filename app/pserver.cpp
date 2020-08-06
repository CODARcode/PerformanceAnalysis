//The parameter server main program. This program collects statistics from the node-instances of the anomaly detector
#include <chimbuko/pserver.hpp>
#ifdef _USE_MPINET
#include <chimbuko/net/mpi_net.hpp>
#else
#include <chimbuko/net/zmq_net.hpp>
#endif
#include <mpi.h>
#include <chimbuko/param/sstd_param.hpp>
#include <chimbuko/util/commandLineParser.hpp>
#include <fstream>

using namespace chimbuko;

struct pserverArgs{
  int nt;
  int n_ad_modules;
  std::string logdir;
  std::string ws_addr;

  pserverArgs(): nt(-1), n_ad_modules(0), logdir("."), ws_addr(""){}

  static commandLineParser<pserverArgs> &getParser(){
    static bool init = false;
    static commandLineParser<pserverArgs> p;
    if(!init){
      addMandatoryCommandLineArg(p, n_ad_modules, "Set the number of AD modules (ranks)");      

      addOptionalCommandLineArg(p, nt, "Set the number of RPC handler threads (max-2 by default)");
      addOptionalCommandLineArg(p, logdir, "Set the output log directory (default: job directory)");
      addOptionalCommandLineArg(p, ws_addr, "Provide the address of the visualization module (aka webserver). If not provided no information will be sent to the visualization");
      init = true;
    }
    return p;
  }
};


int main (int argc, char ** argv){
  pserverArgs args;
  if(argc < pserverArgs::getParser().nMandatoryArgs()+1 || (argc == 2 && std::string(argv[1]) == "-help") ){
    pserverArgs::getParser().help(std::cout);
    return argc < pserverArgs::getParser().nMandatoryArgs()+1 ? 1 : 0;
  }
  pserverArgs::getParser().parse(args, argc-1, (const char**)(argv+1));

  SstdParam param; //global collection of parameters used to identify anomalies
  GlobalAnomalyStats global_func_stats; //global anomaly statistics
  GlobalCounterStats global_counter_stats; //global counter statistics
  PSglobalFunctionIndexMap global_func_index_map; //mapping of function name to global index
    
#ifdef _USE_MPINET
  int provided;
  MPINet net;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
#else
  ZMQNet net;
  MPI_Init(&argc, &argv);
#endif

  PSstatSender stat_sender;

  try {
    if (args.nt <= 0) {
      args.nt = std::max(
		    (int)std::thread::hardware_concurrency() - 5,
		    1
		    );
    }

    //nt = std::max((int)std::thread::hardware_concurrency() - 2, 1);
    std::cout << "Run parameter server with " << args.nt << " threads" << std::endl;
    if (args.ws_addr.size() && args.n_ad_modules){
      std::cout << "Run anomaly statistics sender (ws @ " << args.ws_addr << ")." << std::endl;
      global_func_stats.reset_anomaly_stat({args.n_ad_modules});
    }

    net.add_payload(new NetPayloadUpdateParams(&param));
    net.add_payload(new NetPayloadGetParams(&param));
    net.add_payload(new NetPayloadUpdateAnomalyStats(&global_func_stats));
    net.add_payload(new NetPayloadUpdateCounterStats(&global_counter_stats));
    net.add_payload(new NetPayloadGlobalFunctionIndexMap(&global_func_index_map));
    net.init(nullptr, nullptr, args.nt);

    //Start sending anomaly statistics to viz
    stat_sender.add_payload(new PSstatSenderGlobalAnomalyStatsPayload(&global_func_stats));
    stat_sender.add_payload(new PSstatSenderGlobalCounterStatsPayload(&global_counter_stats));
    stat_sender.run_stat_sender(args.ws_addr);

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
    //param.show(std::cout);
    std::ofstream o;
    o.open(args.logdir + "/parameters.txt");
    if (o.is_open())
      {
	param.show(o);
	o.close();
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

  std::cout << "Pserver finalizing the network" << std::endl;
  net.finalize();
#ifdef _USE_ZMQNET
  MPI_Finalize();
#endif
  std::cout << "Pserver finished" << std::endl;
  return 0;
}
