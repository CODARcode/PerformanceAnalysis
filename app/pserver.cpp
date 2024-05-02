//The parameter server main program. This program collects statistics from the node-instances of the anomaly detector
#include <csignal>
#include <chimbuko_config.h>
#include <chimbuko/pserver.hpp>

#ifdef _USE_MPINET
#include <chimbuko/core/net/mpi_net.hpp>
#else
#include <chimbuko/core/net/zmq_net.hpp>
#endif

#ifdef ENABLE_PROVDB
#include <chimbuko/pserver/PSProvenanceDBclient.hpp>
#endif

#include <chimbuko/core/param/sstd_param.hpp>
#include <chimbuko/core/util/commandLineParser.hpp>
#include <chimbuko/core/util/error.hpp>
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

  int model_update_freq; //frequency in ms at which the global model is updated
  bool model_force_update; //force the global model to be updated every time a worker thread updates its model

#ifdef _USE_ZMQNET
  int max_pollcyc_msg;
  int zmq_io_thr;
  bool autoshutdown;
#endif

#ifdef ENABLE_PROVDB
  std::string provdb_addr_dir;
  std::string provdb_mercury_auth_key; //An authorization key for initializing Mercury (optional, default "")
#endif

  std::string prov_outputpath;

  pserverArgs(): ad("hbos"), nt(-1), logdir("."), ws_addr(""), load_params_set(false), save_params_set(false), freeze_params(false), stat_send_freq(1000), stat_outputdir(""), port(5559), prov_outputpath(""), model_update_freq(1000), model_force_update(false)
#ifdef _USE_ZMQNET
	       , max_pollcyc_msg(10), zmq_io_thr(1), autoshutdown(true)
#endif
#ifdef ENABLE_PROVDB
	       , provdb_addr_dir(""), provdb_mercury_auth_key("")
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
      addOptionalCommandLineArg(p, provdb_addr_dir, "The directory containing the address file written out by the provDB server. An empty string will disable the connection to the global DB.  (default empty, disabled)");
      addOptionalCommandLineArg(p, provdb_mercury_auth_key, "Set the Mercury authorization key for connection to the provDB (default \"\")");
#endif
      addOptionalCommandLineArg(p, prov_outputpath, "Output global provenance data to this directory. Can be used in place of or in conjunction with the provenance database. An empty string \"\" (default) disables this output");
      addOptionalCommandLineArg(p, model_update_freq, "The frequency in ms at which the global AD model is updated (default 1000ms)");
      addOptionalCommandLineArg(p, model_force_update, "Force the global AD model to be updated every time a worker thread updates its model (default false)");

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

  //If number of threads is not specified, choose a sensible number
  if (args.nt <= 0){
    args.nt = std::max(  (int)std::thread::hardware_concurrency() - 5,  1 );
  }

  //Parse environment variables
  if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE")){
    progressStream << "Pserver: Enabling verbose debug output" << std::endl;
    enableVerboseLogging() = true;
  }

  PSparamManager param(args.nt, args.ad); //the AD model; independent models for each worker thread that are aggregated periodically to a global model
  param.enableForceUpdate(args.model_force_update); //decide whether the model is forced to be updated every time a worker updates its model

  std::vector<GlobalAnomalyStats> global_func_stats(args.nt); //global anomaly statistics
  std::vector<GlobalCounterStats> global_counter_stats(args.nt); //global counter statistics
  std::vector<GlobalAnomalyMetrics> global_anom_metrics(args.nt); //global anomaly metrics
  PSglobalFunctionIndexMap global_func_index_map; //mapping of function name to global index

  //Optionally load previously-computed AD algorithm statistics
  if(args.load_params_set){
    progressStream << "Pserver: Loading parameters from input file " << args.load_params << std::endl;
    restoreModel(global_func_index_map,  param, args.load_params);
  }

  //Start the aggregator thread for the global model
  param.setGlobalModelUpdateFrequency(args.model_update_freq);
  param.startUpdaterThread();

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
  if(args.provdb_mercury_auth_key != ""){
    progressStream << "Pserver: setting Mercury authorization key to \"" << args.provdb_mercury_auth_key << "\"" << std::endl;
    ADProvenanceDBengine::setMercuryAuthorizationKey(args.provdb_mercury_auth_key);
  }
  
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
    progressStream << "PServer: Run parameter server ";
#ifdef _USE_ZMQNET
    std::cout << "on port " << args.port << " ";
#endif
    std::cout << "with " << args.nt << " threads" << std::endl;

    if (args.ws_addr.size() || args.stat_outputdir.size()){
      progressStream << "PServer: Run anomaly statistics sender ";
      if(args.ws_addr.size()) std::cout << "(ws @ " << args.ws_addr << ")";
      if(args.stat_outputdir.size()) std::cout << "(dir @ " << args.stat_outputdir << ")";
      std::cout << std::endl;
    }

    for(int i=0;i<args.nt;i++){
      net.add_payload(new NetPayloadUpdateParamManager(&param, i, args.freeze_params), i);
      net.add_payload(new NetPayloadGetParamsFromManager(&param),i);
      net.add_payload(new NetPayloadRecvCombinedADdataArray(&global_func_stats[i], &global_counter_stats[i], &global_anom_metrics[i]),i); //each worker thread writes to a separate stats object which are aggregated only at viz send time
      net.add_payload(new NetPayloadGlobalFunctionIndexMapBatched(&global_func_index_map),i);
      net.add_payload(new NetPayloadPing,i);
    }

    net.init(nullptr, nullptr, args.nt);

    //Start sending anomaly statistics to viz
    stat_sender.add_payload(new PSstatSenderGlobalAnomalyStatsCombinePayload(global_func_stats));
    stat_sender.add_payload(new PSstatSenderGlobalCounterStatsCombinePayload(global_counter_stats));
    stat_sender.add_payload(new PSstatSenderGlobalAnomalyMetricsCombinePayload(global_anom_metrics));
    stat_sender.add_payload(new PSstatSenderCreatedAtTimestampPayload); //add 'created_at' timestamp
    stat_sender.add_payload(new PSstatSenderVersionPayload); //add 'created_at' timestamp
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

    //Explicitly update global model and close the ps manager gather thread
    param.updateGlobalModel();
    param.stopUpdaterThread();

#ifdef ENABLE_PROVDB
    //Send final statistics to the provenance database and/or disk
    if(provdb_client.isConnected() || args.prov_outputpath.size() > 0){
      GlobalCounterStats tmp_cstats; for(int i=0;i<args.nt;i++) tmp_cstats += global_counter_stats[i];
      nlohmann::json global_counter_stats_j = tmp_cstats.get_json_state();

      GlobalAnomalyStats tmp_fstats; for(int i=0;i<args.nt;i++) tmp_fstats += global_func_stats[i];
      GlobalAnomalyMetrics tmp_metrics; for(int i=0;i<args.nt;i++) tmp_metrics += global_anom_metrics[i];

      //Generate the function profile
      FunctionProfile profile;
      tmp_fstats.get_profile_data(profile);
      tmp_metrics.get_profile_data(profile);
      nlohmann::json profile_j = profile.get_json();
      
      //Get the AD model
      std::unique_ptr<ParamInterface> p(ParamInterface::set_AdParam(args.ad));
      p->assign(param.getSerializedGlobalModel());
      nlohmann::json ad_model_j = p->get_algorithm_params(global_func_index_map.getFunctionIndexMap());

      if(provdb_client.isConnected()){
	progressStream << "Pserver: sending final statistics to provDB" << std::endl;
	provdb_client.sendMultipleData(profile_j, GlobalProvenanceDataType::FunctionStats);
	provdb_client.sendMultipleData(global_counter_stats_j, GlobalProvenanceDataType::CounterStats);
	provdb_client.sendMultipleData(ad_model_j, GlobalProvenanceDataType::ADModel);
	progressStream << "Pserver: disconnecting from provDB" << std::endl;
	provdb_client.disconnect();
      }
      if(args.prov_outputpath.size() > 0){
	progressStream << "Pserver: writing final statistics to disk at path " << args.prov_outputpath << std::endl;
	std::ofstream gf(args.prov_outputpath + "/global_func_stats.json");
	std::ofstream gc(args.prov_outputpath + "/global_counter_stats.json");
	std::ofstream ad(args.prov_outputpath + "/ad_model.json");
	gf << profile_j.dump();
	gc << global_counter_stats_j.dump();
	ad << ad_model_j.dump();
      }
    }
#endif

    progressStream << "Pserver: Shutdown parameter server ..." << std::endl;

    //Write params to a file
    std::ofstream o(args.logdir + "/parameters.txt");
    if (o.is_open()){
      std::unique_ptr<ParamInterface> p(ParamInterface::set_AdParam(args.ad));
      p->assign(param.getSerializedGlobalModel());
      p->show(o);
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
    writeModel(args.save_params, global_func_index_map, param);
  }

  progressStream << "Pserver: finished" << std::endl;

  return 0;
}
