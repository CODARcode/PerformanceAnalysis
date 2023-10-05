#include<atomic>
#include<chrono>
#include<thread>
#include<cassert>
#include<csignal>
#include<chimbuko/ad/ADProvenanceDBengine.hpp>
#include<chimbuko/verbose.hpp>
#include<chimbuko/util/string.hpp>
#include<chimbuko/util/commandLineParser.hpp>
#include <thallium/serialization/stl/string.hpp>
#include<chimbuko/provdb/setup.hpp>

using namespace chimbuko;

bool stop_wait_loop = false; //trigger breakout of main thread spin loop

int instance;
#define CprogressStream  progressStream << "ProvDB Commit instance " << instance << ": "
#define CverboseStream  verboseStream << "ProvDB Commit instance " << instance << ": "


void termSignalHandler( int signum ) {
  stop_wait_loop = true;
  CprogressStream << "caught SIGTERM, exiting " << std::endl;
}

bool exit_check(thallium::endpoint &server, 
		thallium::remote_procedure &connection_status){
  std::string conn = connection_status.on(server)();
  int connected =-1;
  int a_client_has_connected=-1;
  int pserver_connected=-1;
  int pserver_has_connected=-1;
  std::stringstream of(conn);
  of >> connected >> a_client_has_connected >> pserver_connected >> pserver_has_connected;
  assert(!of.fail());

  CprogressStream << "determined status: clients connected=" << connected << " (a client has connected ? " << a_client_has_connected << ") "
		  << "pserver connected=" << pserver_connected << " (pserver has connected ? " << pserver_has_connected << ")" << std::endl;
  return 
    ( a_client_has_connected && connected == 0 ) &&
    ( !pserver_has_connected || (pserver_has_connected && !pserver_connected) );
}


struct Args{
  std::string addr_file_dir;
  int instance;
  int ninstances;
  int nshards;
  int freq_ms;
  std::string provdb_mercury_auth_key; //An authorization key for initializing Mercury (optional, default "")
  
  Args(): addr_file_dir("."), instance(0), ninstances(1), nshards(1), freq_ms(30000), provdb_mercury_auth_key(""){}
};


int main(int argc, char** argv){  
#ifdef ENABLE_PROVDB

  commandLineParser<Args> parser;
  addMandatoryCommandLineArg(parser, addr_file_dir, "Specify the directory containing the address file");
  addOptionalCommandLineArg(parser, instance, "Specify the server instance (default 0)");
  addOptionalCommandLineArg(parser, ninstances, "Specify the number of server instances (default 1)");
  addOptionalCommandLineArg(parser, nshards, "Specify the total number of database shards (default 1)");
  addOptionalCommandLineArg(parser, freq_ms, "Specify the frequency in ms at which commit is called (default 30000)");
  addOptionalCommandLineArg(parser, provdb_mercury_auth_key, "Set the Mercury authorization key for connection to the provDB (default \"\")");
  
  if(argc-1 < parser.nMandatoryArgs() || (argc == 2 && std::string(argv[1]) == "-help")){
    parser.help(std::cout);
    return 0;    
  }
  Args args;
  parser.parseCmdLineArgs(args, argc, argv);

  instance = args.instance;

  if(args.freq_ms == 0){
    CprogressStream << "freq==0, I am not needed. Done" << std::endl;
    return 0;
  } 

  if(args.provdb_mercury_auth_key != ""){
    CprogressStream << "setting Mercury authorization key to \"" << args.provdb_mercury_auth_key << "\"" << std::endl;
    ADProvenanceDBengine::setMercuryAuthorizationKey(args.provdb_mercury_auth_key);
  }
  
  ProvDBsetup setup(args.nshards, args.ninstances);
  std::string addr = setup.getInstanceAddress(args.instance, args.addr_file_dir);
  int instance_shard_offset = setup.getShardOffsetInstance(args.instance);
  int nshard_instance = setup.getNshardsInstance(args.instance);
  bool do_global_db = args.instance == setup.getGlobalDBinstance();

  std::string protocol = ADProvenanceDBengine::getProtocolFromAddress(addr);
  if(ADProvenanceDBengine::getProtocol().first != protocol){
    int mode = ADProvenanceDBengine::getProtocol().second;
    CverboseStream << "DB client reinitializing engine with protocol \"" << protocol << "\"" << std::endl;
    ADProvenanceDBengine::finalize();
    ADProvenanceDBengine::setProtocol(protocol,mode);      
  }      

  thallium::engine &eng = ADProvenanceDBengine::getEngine();

  thallium::endpoint server = eng.lookup(addr);

  thallium::remote_procedure client_hello(eng.define("committer_hello").disable_response());
  thallium::remote_procedure client_goodbye(eng.define("committer_goodbye").disable_response());
  thallium::remote_procedure connection_status(eng.define("connection_status"));
  client_hello.on(server)();

  {
    sonata::Client client = sonata::Client(eng);
    {    
      std::vector<sonata::Database> databases(nshard_instance);
      for(int i=0;i<nshard_instance;i++){
	int shard = instance_shard_offset + i;
	databases[i] = client.open(addr, setup.getShardProviderIndex(shard), setup.getShardDBname(shard) );
      }
      std::unique_ptr<sonata::Database> glob_db;
      if(do_global_db) glob_db.reset(new sonata::Database(client.open(addr, setup.getGlobalDBproviderIndex(), setup.getGlobalDBname() )));

      typedef std::chrono::high_resolution_clock Clock;
      Clock::time_point commit_timer_start = Clock::now();

      signal(SIGTERM, termSignalHandler);  
      CprogressStream << "starting commit loop" << std::endl;    

      int iter = 0;
      while(!stop_wait_loop) { //stop wait loop will be set by SIGTERM handler
	thallium::thread::sleep(eng, 1000); //Thallium engine sleeps but listens for rpc requests

	//Check every 10 seconds for connection status
	if(iter % 10 == 0 &&
	   exit_check(server, connection_status ) ){
	  CprogressStream << "no clients connected, exiting" << std::endl;
	  break;
	}

	//Call commit
	unsigned long commit_timer_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - commit_timer_start).count();
	if(commit_timer_ms >= args.freq_ms){
	  CverboseStream << "committing database to disk" << std::endl;
	  for(int s=0;s<nshard_instance;s++){		
	    CprogressStream << "committing shard " << instance_shard_offset + s << std::endl;
	    databases[s].commit();
	  }
	  if(do_global_db){
	    CprogressStream << "committing global db" << std::endl;
	    glob_db->commit();
	  }
	  commit_timer_start = Clock::now();
	}

	++iter;
      }//while(!stop_wait_loop)
    } //db scope
  }//client scope

  CprogressStream << "sending goodbye handshake" << std::endl;
  client_goodbye.on(server)();
  client_hello.deregister();
  client_goodbye.deregister();
  connection_status.deregister();
#endif

  CprogressStream << "done" << std::endl;
  return 0;
}
