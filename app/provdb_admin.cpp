//The "admin" task that creates the provenance database

#include <chimbuko_config.h>
#ifndef ENABLE_PROVDB
#error "Provenance DB build is not enabled"
#endif
#include "chimbuko/util/commandLineParser.hpp"
#include "chimbuko/util/string.hpp"
#include "chimbuko/util/time.hpp"
#include <iostream>
#include <sonata/Admin.hpp>
#include <sonata/Provider.hpp>
#include <sonata/Client.hpp>
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
  progressStream << "ProvDB Admin: Client " << rank << " has said hello: " << connected.size() << " ranks now connected" << std::endl;
}
//Allows a client to deregister from the provider
void client_goodbye(const tl::request& req, const int rank) {
  std::lock_guard<tl::mutex> lock(*mtx);
  connected.erase(rank);
  progressStream << "ProvDB Admin: Client " << rank << " has said goodbye: " << connected.size() << " ranks now connected" << std::endl;
}

//Allows the pserver to register with the provider
void pserver_hello(const tl::request& req) {
  std::lock_guard<tl::mutex> lock(*mtx);
  pserver_connected = true;
  pserver_has_connected = true;
  progressStream << "ProvDB Admin: Pserver has said hello" << std::endl;
}
//Allows the pserver to deregister from the provider
void pserver_goodbye(const tl::request& req) {
  std::lock_guard<tl::mutex> lock(*mtx);
  pserver_connected = false;
  progressStream << "ProvDB Admin: Pserver has said goodbye" << std::endl;
}

//Allows the committer to register with the provider
void committer_hello(const tl::request& req) {
  std::lock_guard<tl::mutex> lock(*mtx);
  committer_connected = true;
  committer_has_connected = true;
  progressStream << "ProvDB Admin: Committer has said hello" << std::endl;
}
//Allows the committer to deregister from the provider
void committer_goodbye(const tl::request& req) {
  std::lock_guard<tl::mutex> lock(*mtx);
  committer_connected = false;
  progressStream << "ProvDB Admin: Committer has said goodbye" << std::endl;
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
  progressStream << "ProvDB Admin: Received shutdown request from client" << std::endl;
}

margo_instance_id margo_id;

#ifdef ENABLE_MARGO_STATE_DUMP
void margo_dump(const std::string &stub){
  std::string fn = stub + "." + getDateTimeFileExt();
  progressStream << "ProvDB Admin: margo dump to " << fn << std::endl;
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
  int nshards;
  int nthreads;
  std::string db_type;
  unsigned long db_commit_freq;
  std::string db_write_dir;
  bool db_in_mem; //database is in-memory not written to disk, for testing
  std::string db_base_config;
  std::string db_margo_config;
  
  ProvdbArgs(): engine("ofi+tcp"), autoshutdown(true), nshards(1), db_type("unqlite"), nthreads(1), db_commit_freq(10000), db_write_dir("."), db_in_mem(false), db_base_config(""), db_margo_config(""){}
};



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
    addOptionalCommandLineArg(parser, nthreads, "Specify the number of RPC handler threads (default 1)");
    addOptionalCommandLineArg(parser, db_type, "Specify the Sonata database type (default \"unqlite\")");
    addOptionalCommandLineArg(parser, db_commit_freq, "Specify the frequency at which the database flushes to disk in ms (default 10000). 0 disables the flush until the end.");
    addOptionalCommandLineArg(parser, db_write_dir, "Specify the directory in which the database shards will be written (default \".\")");
    addOptionalCommandLineArg(parser, db_in_mem, "Use an in-memory database rather than writing to disk (*unqlite backend only*) (default false)");
    addOptionalCommandLineArg(parser, db_base_config, "Provide the *absolute path* to a JSON file to use as the base configuration of the Sonata databases. The database path will be appended automatically (default \"\" - not used)");
    addOptionalCommandLineArg(parser, db_margo_config, "Provide the *absolute path* to a JSON file containing the Margo configuration (default \"\" - not used)");

    if(argc-1 < parser.nMandatoryArgs() || (argc == 2 && std::string(argv[1]) == "-help")){
      parser.help(std::cout);
      return 0;
    }

    ProvdbArgs args;
    parser.parseCmdLineArgs(args, argc, argv);

    if(args.nshards < 1) throw std::runtime_error("Must have at least 1 database shard");
    if(args.db_in_mem && args.db_type != "unqlite") throw std::runtime_error("-db_in_mem option not valid for backends other than unqlite");

    if(args.db_in_mem){ progressStream << "Using in-memory database" << std::endl; }

    std::string eng_opt = args.engine;
    if(args.ip.size() > 0){
      eng_opt += std::string("://") + args.ip;
    }

    //Get Sonata config
    nlohmann::json base_config;
    if(args.db_base_config.size() > 0){
      std::ifstream in(args.db_base_config);
      if(in.fail()){ throw std::runtime_error("Failed to read db config file " + args.db_base_config); }
      base_config = nlohmann::json::parse(in);
    }

    progressStream << "ProvDB Admin: initializing thallium with address: " << eng_opt << std::endl;

    //Initialize margo once to get initial configuration
    margo_id = margo_init(eng_opt.c_str(), MARGO_SERVER_MODE, 0, -1);
    char* config = margo_get_config(margo_id);

    std::cout << "Initial config\n" << config << std::endl;

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
    static const int shard_pool_offset = 2;

    nlohmann::json pool_templ = { {"access","mpmc"}, {"kind","fifo_wait"}, {"name","POOL_NAME"} };
    nlohmann::json xstream_templ = { {"affinity",nlohmann::json::array()} , {"cpubind",-1}, {"name","STREAM_NAME"}, {"scheduler", { {"pools",nlohmann::json::array({0})}, {"type","basic_wait"} }  }   };

    //Global pool
    {
      nlohmann::json pool = pool_templ;
      pool["name"] = "pool_glob";
      config_j["argobots"]["pools"].push_back(pool);

      nlohmann::json xstream = xstream_templ;
      xstream["name"] = "stream_glob";
      xstream["scheduler"]["pools"][0] = glob_pool_idx;
	
      config_j["argobots"]["xstreams"].push_back(xstream);
    }      

    //Pools for shards
    for(int s=0;s<args.nshards;s++){
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

    std::cout << "New config\n" << new_config << std::endl;

    margo_init_info margo_args; memset(&margo_args, 0, sizeof(margo_init_info));
    margo_args.json_config   = new_config.c_str();
    margo_id = margo_init_ext(eng_opt.c_str(), MARGO_SERVER_MODE, &margo_args);

    //Get the thallium pools to pass to the providers
    tl::pool glob_pool;
    {
      ABT_pool p;
      assert(margo_get_pool_by_index(margo_id, glob_pool_idx, &p) == 0);
      glob_pool = tl::pool(p);
    }

    std::vector<tl::pool> shard_pools(args.nshards);
    for(int s=0;s<args.nshards;s++){
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

    std::string addr = (std::string)engine.self();  //ip and port of admin

    { //Scope in which provider is active

      //Initialize providers
      static const int glob_provider_idx = 0;
      static const int shard_provider_offset = 1;

      sonata::Provider glob_provider(engine, glob_provider_idx, "", glob_pool);

      std::vector<std::unique_ptr<sonata::Provider> > shard_providers(args.nshards);
      for(int s=0;s<args.nshards;s++)
	shard_providers[s].reset(new sonata::Provider(engine, s+shard_provider_offset, "", shard_pools[s]));
						  
      progressStream << "ProvDB Admin: Provider is running on " << addr << std::endl;

      { //Scope in which admin object is active
	sonata::Admin admin(engine);
	std::string glob_db_name = "provdb.global";

	nlohmann::json glob_db_config = base_config;
	glob_db_config["path"] = args.db_in_mem ? ":mem:" : stringize("%s/%s.unqlite", args.db_write_dir.c_str(), glob_db_name.c_str());
	glob_db_config["mutex"] = "none"; //as we have only 1 ES per DB we don't need a mutex

	progressStream << "ProvDB Admin: creating global data database: " << glob_db_name << " " << glob_db_config.dump() << " " << args.db_type << std::endl;
	admin.createDatabase(addr, glob_provider_idx, glob_db_name, args.db_type, glob_db_config.dump());
	
	progressStream << "ProvDB Admin: creating " << args.nshards << " database shards" << std::endl;

	std::vector<std::string> db_shard_names(args.nshards);
	for(int s=0;s<args.nshards;s++){
	  std::string db_name = stringize("provdb.%d",s);

	  nlohmann::json config = base_config;
	  config["path"] = args.db_in_mem ? ":mem:" : stringize("%s/%s.unqlite", args.db_write_dir.c_str(), db_name.c_str());
	  config["mutex"] = "none";

	  progressStream << "ProvDB Admin: Shard " << s << ": " << db_name << " " << config.dump() << " " << args.db_type << std::endl;
	  admin.createDatabase(addr, s+shard_provider_offset, db_name, args.db_type, config.dump());
	  db_shard_names[s] = db_name;
	}

	//Create the collections
	{ //scope in which client is active
	  sonata::Client client(engine);

	  //Initialize the provdb shards
	  std::vector<sonata::Database> db(args.nshards);
	  for(int s=0;s<args.nshards;s++){
	    db[s] = client.open(addr, s+shard_provider_offset, db_shard_names[s]);
	    db[s].create("anomalies");
	    db[s].create("metadata");
	    db[s].create("normalexecs");
	  }

	  //Initialize the global provdb
	  sonata::Database glob_db = client.open(addr, glob_provider_idx, glob_db_name);
	  glob_db.create("func_stats");
	  glob_db.create("counter_stats");

	  progressStream << "ProvDB Admin: initialized collections" << std::endl;

	  //Write address to file; do this after initializing collections so that the existence of the file can be used to signal readiness
	  {
	    std::ofstream f("provider.address");
	    f << addr;
	  }

	  //Setup a timer to periodically force a database commit
	  typedef std::chrono::high_resolution_clock Clock;
	  Clock::time_point commit_timer_start = Clock::now();

	  //Spin quietly until SIGTERM sent
	  signal(SIGTERM, termSignalHandler);
	  progressStream << "ProvDB Admin: main thread waiting for completion" << std::endl;
	  while(!stop_wait_loop) { //stop wait loop will be set by SIGTERM handler
	    tl::thread::sleep(engine, 1000); //Thallium engine sleeps but listens for rpc requests

	    unsigned long commit_timer_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - commit_timer_start).count();
	    if(args.db_commit_freq > 0 && commit_timer_ms >= args.db_commit_freq){
	      verboseStream << "ProvDB Admin: committing database to disk" << std::endl;
	      for(int s=0;s<args.nshards;s++){		
		progressStream << "ProvDB Admin: committing shard " << s << std::endl;
		db[s].commit();
	      }
	      progressStream << "ProvDB Admin: committing global db" << std::endl;
	      glob_db.commit();
	      commit_timer_start = Clock::now();
	    }

	    //If at least one client has previously connected but none are now connected, shutdown the server
	    //If all clients disconnected we must also wait for the pserver to disconnect (if it is connected)

	    //If args.autoshutdown is disabled we can force shutdown via a "stop_server" RPC
	    if(
	       (args.autoshutdown || cmd_shutdown)  &&
	       ( a_client_has_connected && connected.size() == 0 ) &&
	       ( !pserver_has_connected || (pserver_has_connected && !pserver_connected) ) &&
	       ( !committer_has_connected || (committer_has_connected && !committer_connected) )
	       ){
	      progressStream << "ProvDB Admin: detected all clients disconnected, shutting down" << std::endl;
#ifdef ENABLE_MARGO_STATE_DUMP
	      margo_dump("margo_dump_all_client_disconnected");
#endif
	      break;
	    }
	  }
	}//client scope

	//If the pserver didn't connect (it is optional), delete the empty database
	if(!pserver_has_connected){
	  progressStream << "ProvDB Admin: destroying pserver database as it didn't connect (connection is optional)" << std::endl;
	  admin.destroyDatabase(addr, glob_provider_idx, glob_db_name);
	}

	progressStream << "ProvDB Admin: ending admin scope" << std::endl;
#ifdef ENABLE_MARGO_STATE_DUMP
	margo_dump("margo_dump_end_admin_scope");
#endif
      }//admin scope

      progressStream << "ProvDB Admin: ending provider scope" << std::endl;           
#ifdef ENABLE_MARGO_STATE_DUMP
      margo_dump("margo_dump_end_provide_scope");
#endif
    }//provider scope

    progressStream << "ProvDB Admin: shutting down server engine" << std::endl;
    delete mtx; //delete mutex prior to engine finalize
    engine.finalize();
    progressStream << "ProvDB Admin: finished, exiting engine scope" << std::endl;
  }
  progressStream << "ProvDB Admin: finished, exiting main scope" << std::endl;
  return 0;
}
