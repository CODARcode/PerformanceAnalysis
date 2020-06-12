//The "admin" task that creates the provenance database

#include <config.h>
#ifndef ENABLE_PROVDB
#error "Provenance DB build is not enabled"
#endif

#include <iostream>
#include <sonata/Admin.hpp>
#include <sonata/Provider.hpp>
#include <sonata/Client.hpp>
#include <unistd.h>
#include <fstream>

#include <iostream>
#include <csignal>
#include <cassert>

namespace tl = thallium;

bool stop_wait_loop = false;
bool engine_is_finalized = false;

void termSignalHandler( int signum ) {
  stop_wait_loop = true;
}


int main(int argc, char** argv) {
  assert(argc <= 2);

  //argv[1] optionally specify the ip address and port (the only way to fix the port that I'm aware of)
  //Should be of form <ip address>:<port>   eg. 127.0.0.1:1234
  std::string eng_opt = "ofi+tcp";
  if(argc == 2){
    eng_opt += std::string("://") + argv[1];
  }

  //Initialize provider engine
  tl::engine engine(eng_opt, THALLIUM_SERVER_MODE);
  engine.enable_remote_shutdown();

  engine.push_finalize_callback([]() { engine_is_finalized = true; stop_wait_loop = true; });

  //Write address to file
  std::string addr = (std::string)engine.self();    
  {
    std::ofstream f("provider.address");
    f << addr;
  }

  //Initialize provider
  sonata::Provider provider(engine);

  std::cout << "Provider is running on " << addr << std::endl;

  {
    std::string config = "{ \"path\" : \"./provdb.unqlite\" }";

    sonata::Admin admin(engine);
    std::cout << "Admin creating database" << std::endl;
    admin.createDatabase(addr, 0, "provdb", "unqlite", config);

    //Create the collections
    {
      sonata::Client client(engine);
      sonata::Database db = client.open(addr, 0, "provdb");
      db.create("anomalies");
      db.create("metadata");
      std::cout << "Admin initialized collections" << std::endl;
    }

    //Spin quietly until SIGTERM sent
    signal(SIGTERM, termSignalHandler);  
    std::cout << "Admin waiting for SIGTERM" << std::endl;
    while(!stop_wait_loop) {
      tl::thread::sleep(engine, 1000); //Thallium engine sleeps but listens for rpc requests
    }

    std::cout << "Admin detaching database" << std::endl;
    admin.detachDatabase(addr, 0, "provdb");

    std::cout << "Admin shutting down server" << std::endl;
    if(!engine_is_finalized)
      engine.finalize();
    std::cout << "Admin thread finished" << std::endl;
  }
  return 0;
}