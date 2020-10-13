//The "admin" task that creates the provenance database

#include <chimbuko_config.h>
#ifndef ENABLE_PROVDB
#error "Provenance DB build is not enabled"
#endif
#include "chimbuko/util/commandLineParser.hpp"
#include "chimbuko/util/string.hpp"
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

namespace tl = thallium;

bool stop_wait_loop = false;
bool engine_is_finalized = false;

void termSignalHandler( int signum ) {
  stop_wait_loop = true;
}

tl::mutex *mtx;
std::set<int> connected;
bool a_client_has_connected = false;

//Allows a client to register with the provider
void client_hello(const tl::request& req, const int rank) {
  std::lock_guard<tl::mutex> lock(*mtx);
  connected.insert(rank);
  a_client_has_connected = true;
  std::cout << "Client " << rank << " has said hello: " << connected.size() << " ranks now connected" << std::endl;
}
//Allows a client to deregister from the provider
void client_goodbye(const tl::request& req, const int rank) {
  std::lock_guard<tl::mutex> lock(*mtx);
  connected.erase(rank);
  std::cout << "Client " << rank << " has said goodbye: " << connected.size() << " ranks now connected" << std::endl;
}

struct ProvdbArgs{
  std::string ip;
  std::string engine;
  bool autoshutdown;
  int nshards;
  int nthreads;
  std::string db_type;
  unsigned long db_commit_freq;

  ProvdbArgs(): engine("ofi+tcp"), autoshutdown(true), nshards(1), db_type("unqlite"), nthreads(1), db_commit_freq(10000){}
};



int main(int argc, char** argv) {
  //argv[1] should specify the ip address and port (the only way to fix the port that I'm aware of)
  //Should be of form <ip address>:<port>   eg. 127.0.0.1:1234
  //Using an empty string will cause it to default to Mochi's default ip/port
  chimbuko::commandLineParser<ProvdbArgs> parser;
  addMandatoryCommandLineArg(parser, ip, "Specify the ip address and port in the format \"${ip}:${port}\". Using an empty string will cause it to default to Mochi's default ip/port.");
  addOptionalCommandLineArg(parser, engine, "Specify the Thallium/Margo engine type (default \"ofi+tcp\")");
  addOptionalCommandLineArg(parser, autoshutdown, "If enabled the provenance DB server will automatically shutdown when all of the clients have disconnected (default true)");
  addOptionalCommandLineArg(parser, nshards, "Specify the number of database shards (default 1)");
  addOptionalCommandLineArg(parser, nthreads, "Specify the number of RPC handler threads (default 1)");
  addOptionalCommandLineArg(parser, db_type, "Specify the Sonata database type (default \"unqlite\")");
  addOptionalCommandLineArg(parser, db_commit_freq, "Specify the frequency at which the database flushes to disk in ms (default 10000)");

  if(argc-1 < parser.nMandatoryArgs() || (argc == 2 && std::string(argv[1]) == "-help")){
    parser.help(std::cout);
    return 0;
  }

  ProvdbArgs args;
  parser.parseCmdLineArgs(args, argc, argv);

  if(args.nshards < 1) throw std::runtime_error("Must have at least 1 database shard");

  std::string eng_opt = args.engine;
  if(args.ip.size() > 0){
    eng_opt += std::string("://") + args.ip;
  }

  std::cout << "ProvDB initializing thallium with address: " << eng_opt << std::endl;

  //Initialize provider engine
  tl::engine engine(eng_opt, THALLIUM_SERVER_MODE, false, args.nthreads); //third argument specifies that progress loop runs on main thread, which should be fine as this thread spends all its time spinning otherwis

#ifdef _PERF_METRIC
  //Get Margo to output profiling information
  //A .diag and .csv file will be produced in the run directory
  //Description can be found at https://xgitlab.cels.anl.gov/sds/margo/blob/master/doc/instrumentation.md#generating-a-profile-and-topology-graph
  //For timing information, the .csv profile output is more useful. In the table section the even lines give the timings
  //Their format is name, avg, rpc_breadcrumb, addr_hash, origin_or_target, cumulative, _min, _max, count, abt_pool_size_hwm, abt_pool_size_lwm, abt_pool_size_cumulative, abt_pool_total_size_hwm, abt_pool_total_size_lwm, abt_pool_total_size_cumulative
  //Generate a profile plot automatically by running margo-gen-profile in the run directory
  // auto margo_instance = engine.get_margo_instance();
  // margo_diag_start(margo_instance);
  // margo_profile_start(margo_instance);

  //Edit: This doesn't seem very reliable, often not outputing all the data. Use the MARGO_ENABLE_PROFILING for more reliable output
#endif

  engine.enable_remote_shutdown();
  engine.push_finalize_callback([]() { engine_is_finalized = true; stop_wait_loop = true; });

  mtx = new tl::mutex;
  engine.define("client_hello",client_hello).disable_response();
  engine.define("client_goodbye",client_goodbye).disable_response();


  //Write address to file
  std::string addr = (std::string)engine.self();    
  {
    std::ofstream f("provider.address");
    f << addr;
  }

  //Initialize provider
  sonata::Provider provider(engine, 0);

  std::cout << "Provider is running on " << addr << std::endl;

  std::vector<std::string> db_shard_names(args.nshards);

  {
    sonata::Admin admin(engine);
    std::cout << "Admin creating " << args.nshards << " database shards" << std::endl;
    
    for(int s=0;s<args.nshards;s++){
      std::string db_name = chimbuko::stringize("provdb.%d",s);
      std::string config = chimbuko::stringize("{ \"path\" : \"./%s.unqlite\" }", db_name.c_str());
      std::cout << "Shard " << s << ": " << db_name << " " << config << std::endl;
      admin.createDatabase(addr, 0, db_name, args.db_type, config);
      db_shard_names[s] = db_name;
    }

    //Create the collections
    {
      sonata::Client client(engine);
      std::vector<sonata::Database> db(args.nshards);
      for(int s=0;s<args.nshards;s++){
	db[s] = client.open(addr, 0, db_shard_names[s]);
	db[s].create("anomalies");
	db[s].create("metadata");
	db[s].create("normalexecs");
      }
      std::cout << "Admin initialized collections" << std::endl;

      //Setup a timer to periodically force a database commit
      typedef std::chrono::high_resolution_clock Clock;
      Clock::time_point commit_timer_start = Clock::now();


      //Spin quietly until SIGTERM sent
      signal(SIGTERM, termSignalHandler);  
      std::cout << "Admin main thread waiting for completion" << std::endl;
      while(!stop_wait_loop) {
	tl::thread::sleep(engine, 1000); //Thallium engine sleeps but listens for rpc requests

	unsigned long commit_timer_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - commit_timer_start).count();
	if(commit_timer_ms >= args.db_commit_freq){
	  std::cout << "Admin committing database to disk" << std::endl;
	  for(int s=0;s<args.nshards;s++)
	    db[s].commit();
	  commit_timer_start = Clock::now();
	}
      
	//If at least one client has previously connected but none are now connected, shutdown the server
	if(args.autoshutdown && a_client_has_connected && connected.size() == 0){
	  std::cout << "Admin detected all clients disconnected, shutting down" << std::endl;
	  break;
	}
      }
    }


    std::cout << "Admin detaching database shards" << std::endl;
    for(int s=0;s<args.nshards;s++){
      admin.detachDatabase(addr, 0, db_shard_names[s]);
    }

    std::cout << "Admin shutting down server" << std::endl;
    if(!engine_is_finalized)
      engine.finalize();
    std::cout << "Admin thread finished" << std::endl;
  }

  delete mtx;
  
  return 0;
}
