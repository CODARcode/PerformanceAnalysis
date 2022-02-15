//The "admin" task that creates the provenance database

#include <chimbuko_config.h>
#ifndef ENABLE_PROVDB
#error "Provenance DB build is not enabled"
#endif
#include "chimbuko/provdb/setup.hpp"
#include "chimbuko/util/commandLineParser.hpp"
#include "chimbuko/util/string.hpp"
#include "chimbuko/util/time.hpp"
#include "chimbuko/util/error.hpp"
#include <iostream>
#include <thallium.hpp>
#include <yokan/cxx/admin.hpp>
#include <yokan/cxx/server.hpp>
#include <yokan/cxx/client.hpp>
#include <unistd.h>
#include <fstream>
#include <set>
#include <iostream>
#include <csignal>
#include <cassert>
#include <nlohmann/json.hpp>

#include <spdlog/spdlog.h>
#include "chimbuko/verbose.hpp"
#include <thallium/serialization/stl/string.hpp>

namespace tl = thallium;

using namespace chimbuko;

bool stop_wait_loop = false; //trigger breakout of main thread spin loop
int instance; //server instance

#define PSprogressStream  progressStream << "ProvDB Admin instance " << instance << ": "
#define PSverboseStream  verboseStream << "ProvDB Admin instance " << instance << ": "


void termSignalHandler( int signum ) {
  stop_wait_loop = true;
}

tl::mutex *mtx;
std::set<int> connected; //which AD ranks are currently connected
bool a_client_has_connected = false; //has an AD rank previously connected?

bool pserver_connected = false; //is the pserver connected
bool pserver_has_connected = false; //did the pserver ever connect

bool committer_connected = false; //is the committer connected?
bool committer_has_connected = false; //did the committer ever connect?

//Allows a client to register with the provider
void client_hello(const tl::request& req, const int rank) {
  std::lock_guard<tl::mutex> lock(*mtx);
  connected.insert(rank);
  a_client_has_connected = true;
  PSprogressStream << "Client " << rank << " has said hello: " << connected.size() << " ranks now connected" << std::endl;
}
//Allows a client to deregister from the provider
void client_goodbye(const tl::request& req, const int rank) {
  std::lock_guard<tl::mutex> lock(*mtx);
  connected.erase(rank);
  PSprogressStream << "Client " << rank << " has said goodbye: " << connected.size() << " ranks now connected" << std::endl;
}

//Allows the pserver to register with the provider
void pserver_hello(const tl::request& req) {
  std::lock_guard<tl::mutex> lock(*mtx);
  pserver_connected = true;
  pserver_has_connected = true;
  PSprogressStream << "Pserver has said hello" << std::endl;
}
//Allows the pserver to deregister from the provider
void pserver_goodbye(const tl::request& req) {
  std::lock_guard<tl::mutex> lock(*mtx);
  pserver_connected = false;
  PSprogressStream << "Pserver has said goodbye" << std::endl;
}

//Allows the committer to register with the provider
void committer_hello(const tl::request& req) {
  std::lock_guard<tl::mutex> lock(*mtx);
  committer_connected = true;
  committer_has_connected = true;
  PSprogressStream << "Committer has said hello" << std::endl;
}
//Allows the committer to deregister from the provider
void committer_goodbye(const tl::request& req) {
  std::lock_guard<tl::mutex> lock(*mtx);
  committer_connected = false;
  PSprogressStream << "Committer has said goodbye" << std::endl;
}

//Get the connection status as a string of format 
//"<current clients> <a client has connected (0/1)> <pserver connected (0/1)> <pserver has connected (0/1)> <committer connected (0/1)> <commiter has connected (0/1)>"
void connection_status(const tl::request& req) {
  std::lock_guard<tl::mutex> lock(*mtx);
  std::stringstream ss;
  ss << connected.size() << " " << int(a_client_has_connected) << " " 
     << int(pserver_connected) << " " << int(pserver_has_connected) << " "
     << int(committer_connected) << " " << int(committer_has_connected);
  req.respond(ss.str());
}



bool cmd_shutdown = false; //true if a client has requested that the server shut down

void client_stop_rpc(const tl::request& req) {
  std::lock_guard<tl::mutex> lock(*mtx);
  cmd_shutdown = true;
  PSprogressStream << "Received shutdown request from client" << std::endl;
}

margo_instance_id margo_id;

#ifdef ENABLE_MARGO_STATE_DUMP
void margo_dump(const std::string &stub){
  std::string fn = stub + "." + getDateTimeFileExt();
  PSprogressStream << "margo dump to " << fn << std::endl;
  margo_state_dump(margo_id, fn.c_str(), 0, nullptr);
}

void margo_dump_rpc(const tl::request& req) {
  const static std::string stub("margo_dump");
  margo_dump(stub);
}
#endif


struct ProvdbArgs{
  std::string ip;
  std::string engine;
  bool autoshutdown;
  int server_instance;
  int ninstances;
  int nshards;
  std::string db_type;
  std::string db_write_dir;
  std::string db_base_config;
  std::string db_margo_config;
 
  ProvdbArgs(): engine("ofi+tcp"), autoshutdown(true), server_instance(0), ninstances(1), nshards(1), db_type("leveldb"), db_write_dir("."), db_base_config(""), db_margo_config("")
		{}
};


void setDatabasePath(nlohmann::json &config, const std::string &path, const std::string &db_name, const std::string &db_type){
  if(db_type == "leveldb" || db_type == "rocksdb" || db_type == "tkrzw" || db_type == "lmdb" || db_type == "unqlite"){  
    std::string fpath = stringize("%s/%s.%s", path.c_str(), db_name.c_str(),  db_type.c_str());
    config["path"] = fpath;
  }else if(db_type == "berkeleydb"){
    std::string fname = stringize("%s.%s", db_name.c_str(),  db_type.c_str());
    config["home"] = path;
    config["file"] = fname;
  }else if(db_type == "map"){

  }else{
    fatal_error("Unknown database type");
  }	  
}


int main(int argc, char** argv) {
  {
    //Parse environment variables
    if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE")){
      progressStream << "ProvDB Admin: Enabling verbose debug output" << std::endl;
      enableVerboseLogging() = true;
      spdlog::set_level(spdlog::level::trace); //enable logging of Sonata
    }

    //argv[1] should specify the ip address and port (the only way to fix the port that I'm aware of)
    //Should be of form <ip address>:<port>   eg. 127.0.0.1:1234
    //Using an empty string will cause it to default to Mochi's default ip/port
    commandLineParser<ProvdbArgs> parser;
    addMandatoryCommandLineArg(parser, ip, "Specify the ip address and port in the format \"${ip}:${port}\". Using an empty string will cause it to default to Mochi's default ip/port.");
    addOptionalCommandLineArg(parser, engine, "Specify the Thallium/Margo engine type (default \"ofi+tcp\")");
    addOptionalCommandLineArg(parser, autoshutdown, "If enabled the provenance DB server will automatically shutdown when all of the clients have disconnected (default true)");
    addOptionalCommandLineArg(parser, nshards, "Specify the number of database shards (default 1)");
    addOptionalCommandLineArg(parser, db_type, "Specify the Yokan database type (default \"leveldb\")");
    addOptionalCommandLineArg(parser, db_write_dir, "Specify the directory in which the database shards will be written (default \".\")");
    addOptionalCommandLineArg(parser, db_base_config, "Provide the *absolute path* to a JSON file to use as the base configuration of the Yokan databases. The database path will be appended automatically (default \"\" - not used)");
    addOptionalCommandLineArg(parser, db_margo_config, "Provide the *absolute path* to a JSON file containing the Margo configuration (default \"\" - not used)");
    addOptionalCommandLineArgMultiValue(parser, server_instance, "Provide the index of the server instance and the total number of instances (if using more than 1) in the format \"$instance $ninstances\" (default \"0 1\")", server_instance, ninstances);

    if(argc-1 < parser.nMandatoryArgs() || (argc == 2 && std::string(argv[1]) == "-help")){
      parser.help(std::cout);
      return 0;
    }

    ProvdbArgs args;
    parser.parseCmdLineArgs(args, argc, argv);

    //Check arguments
    if(args.nshards < 1) throw std::runtime_error("Must have at least 1 database shard");

    std::string eng_opt = args.engine;
    if(args.ip.size() > 0){
      eng_opt += std::string("://") + args.ip;
    }

    //Compute how the shards are divided over the instances
    ProvDBsetup setup(args.nshards, args.ninstances);
    setup.checkInstance(args.server_instance);
    instance = args.server_instance;

    int nshard_instance = setup.getNshardsInstance(instance);
    int instance_shard_offset = setup.getShardOffsetInstance(instance);
    bool instance_do_global_db = instance == setup.getGlobalDBinstance();

    if(nshard_instance == 0 && !instance_do_global_db){
      PSprogressStream << "Instance has no shards (#instances > #nshards), instance is not needed" << std::endl;
      return 0;
    }

    //Get base Yokan backend configuration
    nlohmann::json base_config;
    if(args.db_base_config.size() > 0){
      std::ifstream in(args.db_base_config);
      if(in.fail()){ throw std::runtime_error("Failed to read db config file " + args.db_base_config); }
      base_config = nlohmann::json::parse(in);
    }else if(args.db_type == "leveldb" || args.db_type == "rocksdb"){
      base_config["error_if_exists"] = true;
      base_config["create_if_missing"] = true;
    }else if(args.db_type == "map"){ //in-memory std::map
      base_config["use_lock"] = false; //each xstream exclusively owns a database
    }else if(args.db_type == "berkeleydb"){
      base_config["type"] = "btree";
      base_config["create_if_missing"] = true;
    }else if(args.db_type == "tkrzw"){
      base_config["type"] = "tree";
    }else if(args.db_type == "lmdb"){
      base_config["create_if_missing"] = true;
      base_config["no_lock"] = true;
    }else if(args.db_type == "unqlite"){
      base_config["mode"] = "create";
      base_config["temporary"] = false;
      base_config["use_abt_lock"] = false;
      base_config["no_unqlite_mutex"] = true;
    }else{
      fatal_error("Unknown database type");
    }

    PSprogressStream << "initializing thallium with address: " << eng_opt << std::endl;

    //Initialize margo once to get initial configuration
    margo_id = margo_init(eng_opt.c_str(), MARGO_SERVER_MODE, 0, -1);
    char* config = margo_get_config(margo_id);

    PSverboseStream << "Initial config\n" << config << std::endl;

    nlohmann::json config_j = nlohmann::json::parse(config);
    
    free(config); //yuck c-strings
    margo_finalize(margo_id);
 
    //Initial number of pools should be 1: the primary pool
    assert(config_j["argobots"]["pools"].size() == 1);
    
    //Initial number of streams should be 1: the primary bound to pool 0
    assert(config_j["argobots"]["xstreams"].size() == 1);
    assert(config_j["argobots"]["xstreams"][0]["scheduler"]["pools"].size() == 1);
    assert(config_j["argobots"]["xstreams"][0]["scheduler"]["pools"][0] == 0);
    
    //Add a new pool and xstream for each shard, binding the xstream to the pool
    //Also add a pool for the global db
    static const int glob_pool_idx = 1;
    static const int shard_pool_offset = instance_do_global_db ? 2 : 1;

    nlohmann::json pool_templ = { {"access","mpmc"}, {"kind","fifo_wait"}, {"name","POOL_NAME"} };
    nlohmann::json xstream_templ = { {"affinity",nlohmann::json::array()} , {"cpubind",-1}, {"name","STREAM_NAME"}, {"scheduler", { {"pools",nlohmann::json::array({0})}, {"type","basic_wait"} }  }   };

    //Global pool
    if(instance_do_global_db){
      nlohmann::json pool = pool_templ;
      pool["name"] = "pool_glob";
      config_j["argobots"]["pools"].push_back(pool);

      nlohmann::json xstream = xstream_templ;
      xstream["name"] = "stream_glob";
      xstream["scheduler"]["pools"][0] = glob_pool_idx;
	
      config_j["argobots"]["xstreams"].push_back(xstream);
    }      

    //Pools for shards
    for(int s=0; s<nshard_instance;s++){
      nlohmann::json pool = pool_templ;
      pool["name"] = stringize("pool_s%d",s);
      config_j["argobots"]["pools"].push_back(pool);

      nlohmann::json xstream = xstream_templ;
      xstream["name"] = stringize("stream_s%d",s);
      xstream["scheduler"]["pools"][0] = s+shard_pool_offset;
      config_j["argobots"]["xstreams"].push_back(xstream);
    }

    //Get the new config
    std::string new_config = config_j.dump(4);

    PSprogressStream << "Margo config:\n" << new_config << std::endl;

    margo_init_info margo_args; memset(&margo_args, 0, sizeof(margo_init_info));
    margo_args.json_config   = new_config.c_str();
    margo_id = margo_init_ext(eng_opt.c_str(), MARGO_SERVER_MODE, &margo_args);

    //Get the thallium pools to pass to the providers
    tl::pool glob_pool;
    if(instance_do_global_db){
      ABT_pool p;
      assert(margo_get_pool_by_index(margo_id, glob_pool_idx, &p) == 0);
      glob_pool = tl::pool(p);
    }

    std::vector<tl::pool> shard_pools(nshard_instance);
    for(int s=0;s<nshard_instance;s++){
      ABT_pool p;
      assert(margo_get_pool_by_index(margo_id, s+shard_pool_offset, &p) == 0);
      shard_pools[s] = tl::pool(p);
    }

    tl::engine engine(margo_id); 

    mtx = new tl::mutex;
    engine.define("client_hello",client_hello).disable_response();
    engine.define("client_goodbye",client_goodbye).disable_response();
    engine.define("pserver_hello",pserver_hello).disable_response();
    engine.define("pserver_goodbye",pserver_goodbye).disable_response();
    engine.define("committer_hello",committer_hello).disable_response();
    engine.define("committer_goodbye",committer_goodbye).disable_response();
    engine.define("stop_server",client_stop_rpc).disable_response();
    engine.define("connection_status", connection_status);
#ifdef ENABLE_MARGO_STATE_DUMP
    engine.define("margo_dump", margo_dump_rpc).disable_response();
#endif

    tl::endpoint endpoint = engine.self();

    std::string addr = (std::string)endpoint;  //ip and port of admin
    hg_addr_t addr_hg = endpoint.get_addr();

    std::string provider_config = "{}";

    { //Scope in which provider is active

      //Initialize providers
      int glob_provider_idx = setup.getGlobalDBproviderIndex();
      std::unique_ptr<yokan::Provider> glob_provider;
      if(instance_do_global_db) glob_provider.reset(new yokan::Provider(engine.get_margo_instance(), glob_provider_idx, "", provider_config.c_str(), glob_pool.native_handle()));

      std::vector<std::unique_ptr<yokan::Provider> > shard_providers(nshard_instance);
      std::vector<int> shard_provider_indices(nshard_instance);
      for(int s=0;s<nshard_instance;s++){
	int shard = instance_shard_offset + s;
	shard_provider_indices[s] = setup.getShardProviderIndex(shard);
	shard_providers[s].reset(new yokan::Provider(engine.get_margo_instance(), shard_provider_indices[s], "", provider_config.c_str(), shard_pools[s].native_handle()));
      }
						  
      PSprogressStream << "provider is running on " << addr << std::endl;

      { //Scope in which admin object is active
	yokan::Admin admin(engine.get_margo_instance());

	//Setup global databaase
	std::string glob_db_name = setup.getGlobalDBname();
	yk_database_id_t glob_db_id;
	if(instance_do_global_db){
	  nlohmann::json glob_db_config = base_config;
	  setDatabasePath(glob_db_config, args.db_write_dir, glob_db_name, args.db_type);
	  glob_db_config["name"] = glob_db_name;
	  
	  PSprogressStream << "creating global data database: " << glob_db_name << " on provider " << glob_provider_idx << " with config " << glob_db_config.dump() << " and type " << args.db_type << std::endl;
	  glob_db_id = admin.openDatabase(addr_hg, glob_provider_idx, "", args.db_type.c_str(), glob_db_config.dump().c_str());
	}	

	//Setup database shards
	PSprogressStream << "creating " << nshard_instance << " database shards with index offset " << instance_shard_offset << std::endl;

	std::vector<std::string> db_shard_names(nshard_instance);
	std::vector<yk_database_id_t> db_shard_ids(nshard_instance);
	for(int s=0;s<nshard_instance;s++){
	  int shard = instance_shard_offset + s;
	  db_shard_names[s] = setup.getShardDBname(shard);
	  
	  nlohmann::json config = base_config;
	  setDatabasePath(config, args.db_write_dir, db_shard_names[s], args.db_type);
	  config["name"] = db_shard_names[s];

	  PSprogressStream << "shard " << shard << ": " << db_shard_names[s] << " on provider " << shard_provider_indices[s] << " with config " << config.dump() << " and type " << args.db_type << std::endl;
	  db_shard_ids[s] = admin.openDatabase(addr_hg, shard_provider_indices[s], "", args.db_type.c_str(), config.dump().c_str());
	}

	//Create the collections
	{ //scope in which client is active
	  yokan::Client client(engine.get_margo_instance());

	  //Initialize the provdb shards
	  std::vector<yokan::Database> db(nshard_instance);
	  for(int s=0;s<nshard_instance;s++){
	    db[s] = client.makeDatabaseHandle(addr_hg, shard_provider_indices[s], db_shard_ids[s]); 
	    db[s].createCollection("anomalies");
	    db[s].createCollection("metadata");
	    db[s].createCollection("normalexecs");
	  }

	  PSprogressStream << "initialized shard collections" << std::endl;

	  //Initialize the global provdb
	  std::unique_ptr<yokan::Database> glob_db;
	  if(instance_do_global_db){
	    glob_db.reset(new yokan::Database(client.makeDatabaseHandle(addr_hg, glob_provider_idx, glob_db_id)));
	    glob_db->createCollection("func_stats");
	    glob_db->createCollection("counter_stats");
	    PSprogressStream << "initialized global DB collections" << std::endl;
	  }

	  //Instance 0 writes a map of shard index to server instance and provider index for use by the viz
	  if(instance == 0) setup.writeShardInstanceMap();

	  //Write address to file; do this after initializing collections so that the existence of the file can be used to signal readiness
	  {	    
	    std::ofstream f(setup.getInstanceAddressFilename(instance));
	    f << addr;
	  }

	  //Setup a timer to periodically force a database commit
	  typedef std::chrono::high_resolution_clock Clock;
	  Clock::time_point commit_timer_start = Clock::now();

	  //Spin quietly until SIGTERM sent
	  signal(SIGTERM, termSignalHandler);
	  PSprogressStream << "main thread waiting for completion" << std::endl;

	  size_t iter = 0;
	  while(!stop_wait_loop) { //stop wait loop will be set by SIGTERM handler
	    tl::thread::sleep(engine, 1000); //Thallium engine sleeps but listens for rpc requests
	    if(iter % 20 == 0){ verboseStream << "ProvDB Admin heartbeat" << std::endl; }
	    
	    //If at least one client has previously connected but none are now connected, shutdown the server
	    //If all clients disconnected we must also wait for the pserver to disconnect (if it is connected)

	    //If args.autoshutdown is disabled we can force shutdown via a "stop_server" RPC
	    if(
	       (args.autoshutdown || cmd_shutdown)  &&
	       ( a_client_has_connected && connected.size() == 0 ) &&
	       ( !pserver_has_connected || (pserver_has_connected && !pserver_connected) ) &&
	       ( !committer_has_connected || (committer_has_connected && !committer_connected) )
	       ){
	      PSprogressStream << "detected all clients disconnected, shutting down" << std::endl;
#ifdef ENABLE_MARGO_STATE_DUMP
	      margo_dump("margo_dump_all_client_disconnected." + std::to_string(instance));
#endif
	      break;
	    }

	    ++iter;
	  }//wait loop
	}//client scope

#if 0 
	//*****This causes hangs on Summit and is not strictly necessary****
	//If the pserver didn't connect (it is optional), delete the empty database
	if(!pserver_has_connected){
	  PSprogressStream << "destroying pserver database as it didn't connect (connection is optional)" << std::endl;
	  admin.destroyDatabase(addr, glob_provider_idx, glob_db_name);
	}
#endif

	PSprogressStream << "ending admin scope" << std::endl;
#ifdef ENABLE_MARGO_STATE_DUMP
	margo_dump("margo_dump_end_admin_scope." + std::to_string(instance));
#endif
      }//admin scope

      PSprogressStream << "ending provider scope" << std::endl;           
#ifdef ENABLE_MARGO_STATE_DUMP
      margo_dump("margo_dump_end_provide_scope." + std::to_string(instance));
#endif
    }//provider scope

    if( margo_addr_free(engine.get_margo_instance(), addr_hg) != HG_SUCCESS) fatal_error("Could not free address object");

    PSprogressStream << "shutting down server engine" << std::endl;
    delete mtx; //delete mutex prior to engine finalize
    engine.finalize();
    PSprogressStream << "finished, exiting engine scope" << std::endl;
  }
  PSprogressStream << "finished, exiting main scope" << std::endl;
  return 0;
}
