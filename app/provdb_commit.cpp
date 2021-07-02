#include<atomic>
#include<chrono>
#include<thread>
#include<cassert>
#include<chimbuko/ad/ADProvenanceDBengine.hpp>
#include<chimbuko/verbose.hpp>
#include<chimbuko/util/string.hpp>
#include <thallium/serialization/stl/string.hpp>

using namespace chimbuko;

bool stop_wait_loop = false; //trigger breakout of main thread spin loop

void termSignalHandler( int signum ) {
  stop_wait_loop = true;
  progressStream << "ProvDB Commit caught SIGTERM, exiting " << std::endl;
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

  progressStream << "ProvDB Commit determined status: clients connected=" << connected << " (a client has connected ? " << a_client_has_connected << ") "
		 << "pserver connected=" << pserver_connected << " (pserver has connected ? " << pserver_has_connected << ")" << std::endl;
  return 
    ( a_client_has_connected && connected == 0 ) &&
    ( !pserver_has_connected || (pserver_has_connected && !pserver_connected) );
}


int main(int argc, char** argv){  
#ifdef ENABLE_PROVDB
  if(argc != 4){
    std::cout << "Usage: provdb_commit <server address e.g. ofi+tcp;ofi_rxm://172.17.0.4:5000> <nshards> <freq (ms)>" << std::endl;
    return 1;
  }
  std::string addr = argv[1];
  int nshards = std::stoi(argv[2]);
  int freq_ms = std::stoi(argv[3]);

  if(freq_ms == 0){
    progressStream << "ProvDB Commit freq==0, I am not needed. Done" << std::endl;
    return 0;
  } 

  std::string protocol = ADProvenanceDBengine::getProtocolFromAddress(addr);
  if(ADProvenanceDBengine::getProtocol().first != protocol){
    int mode = ADProvenanceDBengine::getProtocol().second;
    verboseStream << "DB client reinitializing engine with protocol \"" << protocol << "\"" << std::endl;
    ADProvenanceDBengine::finalize();
    ADProvenanceDBengine::setProtocol(protocol,mode);      
  }      

  thallium::engine &eng = ADProvenanceDBengine::getEngine();

  thallium::endpoint server = eng.lookup(addr);

  thallium::remote_procedure client_hello(eng.define("committer_hello").disable_response());
  thallium::remote_procedure client_goodbye(eng.define("committer_goodbye").disable_response());
  thallium::remote_procedure connection_status(eng.define("connection_status"));
  client_hello.on(server)();

  static const int glob_provider_idx = 0;
  static const int shard_provider_offset = 1;

  {
    sonata::Client client = sonata::Client(eng);
    {    
      std::vector<sonata::Database> databases(nshards);
      for(int i=0;i<nshards;i++){
	std::string db_name = stringize("provdb.%d", i);
	databases[i] = client.open(addr, i+shard_provider_offset, db_name);
      }
      sonata::Database glob_db  = client.open(addr, glob_provider_idx, "provdb.global");
    
      typedef std::chrono::high_resolution_clock Clock;
      Clock::time_point commit_timer_start = Clock::now();

      signal(SIGTERM, termSignalHandler);  
      progressStream << "ProvDB Commit:  starting commit loop" << std::endl;    

      int iter = 0;
      while(!stop_wait_loop) { //stop wait loop will be set by SIGTERM handler
	thallium::thread::sleep(eng, 1000); //Thallium engine sleeps but listens for rpc requests

	//Check every 10 seconds for connection status
	if(iter % 10 == 0 &&
	   exit_check(server, connection_status ) ){
	  progressStream << "ProvDB Commit: no clients connected, exiting" << std::endl;
	  break;
	}

	unsigned long commit_timer_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - commit_timer_start).count();
	if(commit_timer_ms >= freq_ms){
	  verboseStream << "ProvDB Commit: committing database to disk" << std::endl;
	  for(int s=0;s<nshards;s++){		
	    progressStream << "ProvDB Commit: committing shard " << s << std::endl;
	    databases[s].commit();
	  }
	  progressStream << "ProvDB Commit: committing global db" << std::endl;
	  glob_db.commit();
	  commit_timer_start = Clock::now();
	}
	++iter;
      }//while(!stop_wait_loop)
    } //db scope
  }//client scope

  progressStream << "ProvDB Commit sending goodbye handshake" << std::endl;
  client_goodbye.on(server)();
  client_hello.deregister();
  client_goodbye.deregister();
  connection_status.deregister();
#endif

  progressStream << "ProvDB Commit done" << std::endl;
  return 0;
}
