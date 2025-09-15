//The parameter server main program. This program collects statistics from the node-instances of the anomaly detector
#include <csignal>
#include <chimbuko_config.h>

#ifdef _USE_MPINET
#include <chimbuko/core/net/mpi_net.hpp>
#else
#include <chimbuko/core/net/zmq_net.hpp>
#endif

#ifdef ENABLE_PROVDB
#include <chimbuko/core/pserver/PSglobalProvenanceDBclient.hpp>
#include <chimbuko/core/pserver/PSshardProvenanceDBclient.hpp>
#endif

#include <chimbuko/core/param/sstd_param.hpp>
#include <chimbuko/core/util/commandLineParser.hpp>
#include <chimbuko/core/util/error.hpp>
#include <fstream>
#include <chimbuko/core/verbose.hpp>
#include <chimbuko/core/ad/ADOutlier.hpp>

#include <chimbuko/modules/factory.hpp>

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

  int model_update_freq; //frequency in ms at which the global model is updated
  bool model_force_update; //force the global model to be updated every time a worker thread updates its model

  ADOutlier::AlgoParams algo_params; //The AD algorithm hyperparameters and type/name

#ifdef _USE_ZMQNET
  int max_pollcyc_msg;
  int zmq_io_thr;
  bool autoshutdown;
#endif

#ifdef ENABLE_PROVDB
  std::string provdb_addr_dir;
  int nprovdb_shards; /**< Number of database shards*/
  int nprovdb_instances; /**< Number of instances of the provenance database server*/
  std::string provdb_mercury_auth_key; //An authorization key for initializing Mercury (optional, default "")
  bool provdb_post_prune; //perform post-pruning on the provenance database
#endif

  std::string prov_outputpath;

  pserverArgs(): nt(-1), logdir("."), ws_addr(""), load_params_set(false), save_params_set(false), freeze_params(false), stat_send_freq(1000), stat_outputdir(""), port(5559), prov_outputpath(""), model_update_freq(1000), model_force_update(false)
#ifdef _USE_ZMQNET
	       , max_pollcyc_msg(10), zmq_io_thr(1), autoshutdown(true)
#endif
#ifdef ENABLE_PROVDB
	       , provdb_addr_dir(""), provdb_mercury_auth_key(""), provdb_post_prune(true), nprovdb_shards(1), nprovdb_instances(1)
#endif
  {}

  static commandLineParser &getParser(pserverArgs &instance){
    static bool init = false;
    static commandLineParser p;
    if(!init){
      addOptionalCommandLineArg(p, instance, nt, "Set the number of RPC handler threads (max-2 by default)");
      addOptionalCommandLineArg(p, instance, logdir, "Set the output log directory (default: job directory)");
      addOptionalCommandLineArg(p, instance, port, "Set the pserver port (default: 5559)");
      addOptionalCommandLineArg(p, instance, ws_addr, "Provide the address of the visualization module (aka webserver). If not provided no information will be sent to the visualization");
      addOptionalCommandLineArg(p, instance, stat_outputdir, "Optionally provide a directory where the stat data will be written alongside/in place of sending to the viz module (default: unused");
      addOptionalCommandLineArgWithFlag(p, instance, load_params, load_params_set, "Load previously computed anomaly algorithm parameters from file");
      addOptionalCommandLineArgWithFlag(p, instance, save_params, save_params_set, "Save anomaly algorithm parameters to file");
      addOptionalCommandLineArg(p, instance, freeze_params, "Fix the anomaly algorithm parameters, preventing updates from the AD. Use in conjunction with -load_params. Value should be 'true' or 'false' (or 0/1)");
      addOptionalCommandLineArg(p, instance, stat_send_freq, "The frequency in ms at which statistics are sent to the visualization (default 1000ms)");
#ifdef _USE_ZMQNET
      addOptionalCommandLineArg(p, instance, max_pollcyc_msg, "Set the maximum number of messages that the router thread will route front->back and back->front per poll cycle (default: 10)");
      addOptionalCommandLineArg(p, instance, zmq_io_thr, "Set the number of io threads used by ZeroMQ (default: 1)");
      addOptionalCommandLineArg(p, instance, autoshutdown, "If enabled the pserver will automatically shutdown when all clients have disconnected (default: true)");
#endif
#ifdef ENABLE_PROVDB
      addOptionalCommandLineArg(p, instance, provdb_addr_dir, "The directory containing the address file written out by the provDB server. An empty string will disable the connection to the global DB.  (default empty, disabled)");
      addOptionalCommandLineArg(p, instance, provdb_mercury_auth_key, "Set the Mercury authorization key for connection to the provDB (default \"\")");
      addOptionalCommandLineArg(p, instance, provdb_post_prune, "If enabled the pserver will automatically \"prune\" the provenance database at the end of the run (default: true)");
      addOptionalCommandLineArgWithDefault(p, instance, nprovdb_shards, 1, "Number of provenance database shards. Clients connect to shards round-robin by rank (default 1)");
      addOptionalCommandLineArgWithDefault(p, instance, nprovdb_instances, 1, "Number of provenance database instances. Shards are divided uniformly over instances. (default 1)");
#endif
      addOptionalCommandLineArg(p, instance, prov_outputpath, "Output global provenance data to this directory. Can be used in place of or in conjunction with the provenance database. An empty string \"\" (default) disables this output");
      addOptionalCommandLineArg(p, instance, model_update_freq, "The frequency in ms at which the global AD model is updated (default 1000ms)");
      addOptionalCommandLineArg(p, instance, model_force_update, "Force the global AD model to be updated every time a worker thread updates its model (default false)");

      p.addOptionalArg(new ADOutlier::AlgoParams::cmdlineParser(instance.algo_params, "-algo_params_file", "Set the filename containing the algorithm name and hyperparameters (ensure consistent with OAD)."));
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
  bool do_help = argc < 2;
  for(int i=1;i<argc;i++) if(std::string(argv[i])=="-help"){ do_help = true; break; }

  if(do_help){
    pserverArgs::getParser(args).help(std::cout);
    return 0;
  }
  
  std::string module = argv[1];

  pserverArgs::getParser(args).parse(argc-2, (const char**)(argv+2));

  //If number of threads is not specified, choose a sensible number
  if (args.nt <= 0){
    args.nt = std::max(  (int)std::thread::hardware_concurrency() - 5,  1 );
  }

  //Parse environment variables
  if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE")){
    progressStream << "Pserver: Enabling verbose debug output" << std::endl;
    enableVerboseLogging() = true;
  }

  PSparamManager param(args.nt, args.algo_params.algorithm); //the AD model; independent models for each worker thread that are aggregated periodically to a global model
  param.enableForceUpdate(args.model_force_update); //decide whether the model is forced to be updated every time a worker updates its model

  std::unique_ptr<ProvDBmoduleSetupCore> pdb_setup = modules::factoryInstantiateProvDBmoduleSetup(module);
  std::unique_ptr<PSmoduleDataManagerCore> ps_module_data_man = modules::factoryInstantiatePSmoduleDataManager(module, args.nt);
  
  //Optionally load previously-computed AD algorithm statistics
  if(args.load_params_set){
    progressStream << "Pserver: Loading parameters from input file " << args.load_params << std::endl;
    ps_module_data_man->restoreModel(param, args.load_params);
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
    ProvDBengine::setMercuryAuthorizationKey(args.provdb_mercury_auth_key);
  }
  PSglobalProvenanceDBclient provdb_client(pdb_setup->getGlobalDBcollections());
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
      net.add_payload(new NetPayloadPing,i);
      ps_module_data_man->appendNetWorkerPayloads(net, i);
    }

    net.init(nullptr, nullptr, args.nt);

    //Start sending anomaly statistics to viz
    stat_sender.add_payload(new PSstatSenderCreatedAtTimestampPayload); //add 'created_at' timestamp
    stat_sender.add_payload(new PSstatSenderVersionPayload); //add 'version' timestamp
    ps_module_data_man->appendStatSenderPayloads(stat_sender);

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
    if(provdb_client.isConnected() || args.prov_outputpath.size() > 0) 
      ps_module_data_man->sendFinalModuleDataToProvDB(provdb_client, args.prov_outputpath, param);    
#endif

    progressStream << "Pserver: Shutdown parameter server ..." << std::endl;

    //Write params to a file
    std::ofstream o(args.logdir + "/parameters.txt");
    if (o.is_open()){
      param.getGlobalParamsCopy()->show(o);
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
    ps_module_data_man->writeModel(args.save_params, param);
  }

#ifdef ENABLE_PROVDB
  //Post-prune the provenance database
  if(args.provdb_post_prune && args.provdb_addr_dir.size()){
    progressStream << "PServer: Pruning the provenance database" << std::endl;
    std::unique_ptr<ProvDBpruneCore> pruner = modules::factoryInstantiateProvDBprune(module, args.algo_params, param.getGlobalParamsCopy()->serialize());

    for(int s=0;s<args.nprovdb_shards;s++){
      progressStream << "PServer: Pruning shard " << s+1 << " of " << args.nprovdb_shards << std::endl;
      PSshardProvenanceDBclient shard_client(pdb_setup->getMainDBcollections());
      shard_client.connectShard(s, args.provdb_addr_dir, args.nprovdb_shards, args.nprovdb_instances);
      pruner->prune(shard_client.getDatabase());
    }
    progressStream << "PServer: Updating global function stats" << std::endl;
    pruner->finalize(provdb_client.getDatabase());
  }
  if(provdb_client.isConnected()){
    progressStream << "Pserver: disconnecting from provDB" << std::endl;
    provdb_client.disconnect();
  }
#endif
  
  progressStream << "Pserver: finished" << std::endl;

  return 0;
}
