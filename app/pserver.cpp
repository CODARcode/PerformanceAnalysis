//The parameter server main program. This program collects statistics from the node-instances of the anomaly detector
#include <csignal>
#include <chimbuko_config.h>
#include <chimbuko/pserver.hpp>

#ifdef _USE_MPINET
#include <mpi.h>
#include <chimbuko/net/mpi_net.hpp>
#else
#include <chimbuko/net/zmq_net.hpp>
#endif

#ifdef ENABLE_PROVDB
#include <chimbuko/pserver/PSProvenanceDBclient.hpp>
#endif

#include <chimbuko/param/sstd_param.hpp>
#include <chimbuko/util/commandLineParser.hpp>
#include <chimbuko/util/error.hpp>
#include <fstream>
#include "chimbuko/verbose.hpp"



using namespace chimbuko;

struct pserverArgs{
  int nt;
  std::string logdir;
  std::string ws_addr;

  int port;

  std::string load_params;
  bool load_params_set;

  std::string save_params;
  bool save_params_set;

  bool freeze_params;

  int stat_send_freq;

  std::string stat_outputdir;
  std::string ad;

#ifdef _USE_ZMQNET
  int max_pollcyc_msg;
  int zmq_io_thr;
  bool autoshutdown;
#endif

#ifdef ENABLE_PROVDB
  std::string provdb_addr_dir;
#endif

  std::string prov_outputpath;

  pserverArgs(): ad("hbos"), nt(-1), logdir("."), ws_addr(""), load_params_set(false), save_params_set(false), freeze_params(false), stat_send_freq(1000), stat_outputdir(""), port(5559), prov_outputpath("")
#ifdef _USE_ZMQNET
	       , max_pollcyc_msg(10), zmq_io_thr(1), autoshutdown(true)
#endif
#ifdef ENABLE_PROVDB
	       , provdb_addr_dir(".")
#endif
  {}

  static commandLineParser<pserverArgs> &getParser(){
    static bool init = false;
    static commandLineParser<pserverArgs> p;
    if(!init){
      addOptionalCommandLineArg(p, ad, "Set AD algorithm to use.");
      addOptionalCommandLineArg(p, nt, "Set the number of RPC handler threads (max-2 by default)");
      addOptionalCommandLineArg(p, logdir, "Set the output log directory (default: job directory)");
      addOptionalCommandLineArg(p, port, "Set the pserver port (default: 5559)");
      addOptionalCommandLineArg(p, ws_addr, "Provide the address of the visualization module (aka webserver). If not provided no information will be sent to the visualization");
      addOptionalCommandLineArg(p, stat_outputdir, "Optionally provide a directory where the stat data will be written alongside/in place of sending to the viz module (default: unused");
      addOptionalCommandLineArgWithFlag(p, load_params, load_params_set, "Load previously computed anomaly algorithm parameters from file");
      addOptionalCommandLineArgWithFlag(p, save_params, save_params_set, "Save anomaly algorithm parameters to file");
      addOptionalCommandLineArg(p, freeze_params, "Fix the anomaly algorithm parameters, preventing updates from the AD. Use in conjunction with -load_params. Value should be 'true' or 'false' (or 0/1)");
      addOptionalCommandLineArg(p, stat_send_freq, "The frequency in ms at which statistics are sent to the visualization (default 1000ms)");
#ifdef _USE_ZMQNET
      addOptionalCommandLineArg(p, max_pollcyc_msg, "Set the maximum number of messages that the router thread will route front->back and back->front per poll cycle (default: 10)");
      addOptionalCommandLineArg(p, zmq_io_thr, "Set the number of io threads used by ZeroMQ (default: 1)");
      addOptionalCommandLineArg(p, autoshutdown, "If enabled the pserver will automatically shutdown when all clients have disconnected (default: true)");
#endif
#ifdef ENABLE_PROVDB
      addOptionalCommandLineArg(p, provdb_addr_dir, "The directory containing the address file written out by the provDB server (default \".\"). An empty string will disable the connection to the global DB.");
#endif
      addOptionalCommandLineArg(p, prov_outputpath, "Output global provenance data to this directory. Can be used in place of or in conjunction with the provenance database. An empty string \"\" (default) disables this output");


      init = true;
    }
    return p;
  }
};


//Allow for graceful exit on sigterm
void termSignalHandler( int signum ){
  progressStream << "Caught SIGTERM, shutting down" << std::endl;
}


int main (int argc, char ** argv){
  pserverArgs args;
  if(argc == 2 && std::string(argv[1]) == "-help"){
    pserverArgs::getParser().help(std::cout);
    return 0;
  }
  pserverArgs::getParser().parse(args, argc-1, (const char**)(argv+1));

  //Parse environment variables
  if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE")){
    progressStream << "Pserver: Enabling verbose debug output" << std::endl;
    enableVerboseLogging() = true;
  }

  ParamInterface * param = ParamInterface::set_AdParam(args.ad); //"hbos"); //sstd"); //HbosParam param; //global collection of parameters used to identify anomalies
  if (param == nullptr) {
    fatal_error("INCORRECT algorithm for AdParam: Not Found. Choose sstd or hbos.");
    // verboseStream << "INCORRECT algorithm for AdParam: Not Found. Choose sstd or hbos." << std::endl;
    // exit(EXIT_FAILURE);
  }
  GlobalAnomalyStats global_func_stats; //global anomaly statistics
  GlobalCounterStats global_counter_stats; //global counter statistics
  PSglobalFunctionIndexMap global_func_index_map; //mapping of function name to global index

  //Optionally load previously-computed AD algorithm statistics
  if(args.load_params_set){
    progressStream << "Pserver: Loading parameters from input file " << args.load_params << std::endl;
    std::ifstream in(args.load_params);
    if(!in.good()) throw std::runtime_error("Could not load anomaly algorithm parameters from the file provided");
    nlohmann::json in_p;
    in >> in_p;
    global_func_index_map.deserialize(in_p["func_index_map"]);
    param->assign(in_p["alg_params"].dump());
  }

#ifdef _USE_MPINET
  int provided;
  MPINet net;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
#else
  ZMQNet net;
  net.setMaxMsgPerPollCycle(args.max_pollcyc_msg);
  net.setIOthreads(args.zmq_io_thr);
  net.setPort(args.port);
  net.setAutoShutdown(args.autoshutdown);
#endif

  PSstatSender stat_sender(args.stat_send_freq);

#ifdef ENABLE_PROVDB
  PSProvenanceDBclient provdb_client;
#endif

  try {
#ifdef ENABLE_PROVDB
    //Connect to the provenance database
    if(args.provdb_addr_dir.size()){
      progressStream << "Pserver: connecting to provenance database" << std::endl;
      provdb_client.connectMultiServer(args.provdb_addr_dir);
    }
#endif

    //Setup network
    if (args.nt <= 0) {
      args.nt = std::max(
		    (int)std::thread::hardware_concurrency() - 5,
		    1
		    );
    }

    progressStream << "PServer: Run parameter server ";
#ifdef _USE_ZMQNET
    std::cout << "on port " << args.port << " ";
#endif
    std::cout << "with " << args.nt << " threads" << std::endl;

    if (args.ws_addr.size() || args.stat_outputdir.size()){
      progressStream << "PServer: Run anomaly statistics sender ";
      if(args.ws_addr.size()) std::cout << "(ws @ " << args.ws_addr << ")";
      if(args.stat_outputdir.size()) std::cout << "(dir @ " << args.stat_outputdir << ")";
    }

    net.add_payload(new NetPayloadUpdateParams(param, args.freeze_params));
    net.add_payload(new NetPayloadGetParams(param));
    net.add_payload(new NetPayloadUpdateAnomalyStats(&global_func_stats));
    net.add_payload(new NetPayloadUpdateCounterStats(&global_counter_stats));
    net.add_payload(new NetPayloadGlobalFunctionIndexMapBatched(&global_func_index_map));
    net.init(nullptr, nullptr, args.nt);

    //Start sending anomaly statistics to viz
    stat_sender.add_payload(new PSstatSenderGlobalAnomalyStatsPayload(&global_func_stats));
    stat_sender.add_payload(new PSstatSenderGlobalCounterStatsPayload(&global_counter_stats));
    stat_sender.run_stat_sender(args.ws_addr, args.stat_outputdir);

    //Register a signal handler that prevents the application from exiting on SIGTERM; instead this signal will be handled by ZeroMQ and will cause the pserver to shutdown gracefully
    signal(SIGTERM, termSignalHandler);

    //Start communicating with the AD instances
#ifdef _PERF_METRIC
    net.run(args.logdir);
#else
    net.run();
#endif

    signal(SIGTERM, SIG_DFL); //restore default signal handling

    //At this point, all pseudo AD modules finished sending anomaly statistics data
    std::this_thread::sleep_for(std::chrono::seconds(1));
    stat_sender.stop_stat_sender(1000);

#ifdef ENABLE_PROVDB
    //Send final statistics to the provenance database and/or disk
    if(provdb_client.isConnected() || args.prov_outputpath.size() > 0){
      nlohmann::json global_func_stats_j = global_func_stats.collect_func_data();
      nlohmann::json global_counter_stats_j = global_counter_stats.get_json_state();

      if(provdb_client.isConnected()){
	progressStream << "Pserver: sending final statistics to provDB" << std::endl;
	provdb_client.sendMultipleData(global_func_stats_j, GlobalProvenanceDataType::FunctionStats);
	provdb_client.sendMultipleData(global_counter_stats_j, GlobalProvenanceDataType::CounterStats);
	progressStream << "Pserver: disconnecting from provDB" << std::endl;
	provdb_client.disconnect();
      }
      if(args.prov_outputpath.size() > 0){
	progressStream << "Pserver: writing final statistics to disk at path " << args.prov_outputpath << std::endl;
	std::ofstream gf(args.prov_outputpath + "/global_func_stats.json");
	std::ofstream gc(args.prov_outputpath + "/global_counter_stats.json");
	gf << global_func_stats_j.dump();
	gc << global_counter_stats_j.dump();
      }
    }
#endif

    // could be output to a file
    progressStream << "Pserver: Shutdown parameter server ..." << std::endl;
    //param.show(std::cout);
    std::ofstream o;
    o.open(args.logdir + "/parameters.txt");
    if (o.is_open())
      {
	param->show(o);
	o.close();
      }
  }
  catch (std::invalid_argument &e)
    {
      progressStream << e.what() << std::endl;
    }
  catch (std::ios_base::failure &e)
    {
      progressStream << "I/O base exception caught\n";
      progressStream << e.what() << std::endl;
    }
  catch (std::exception &e)
    {
      progressStream << "Exception caught\n";
      progressStream << e.what() << std::endl;
    }

  progressStream << "Pserver: finalizing the network" << std::endl;
  net.finalize();

  //Optionally save the final AD algorithm parameters
  if(args.save_params_set){
    progressStream << "PServer: Saving parameters to output file " << args.save_params << std::endl;
    std::ofstream out(args.save_params);
    if(!out.good()) throw std::runtime_error("Could not write anomaly algorithm parameters to the file provided");
    nlohmann::json out_p;
    out_p["func_index_map"] = global_func_index_map.serialize();
    out_p["alg_params"] = nlohmann::json::parse(param->serialize());
    out << out_p;
  }

  progressStream << "Pserver: finished" << std::endl;
  return 0;
}
