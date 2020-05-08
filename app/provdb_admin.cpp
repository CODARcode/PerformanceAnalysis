//The "admin" task that creates the provenance database

#include <config.h>
#ifndef ENABLE_PROVDB
#error "Provenance DB build is not enabled"
#endif

#include <iostream>
#include <sonata/Admin.hpp>
#include <sonata/Provider.hpp>
#include <sonata/Client.hpp>
#include <nlohmann/json.hpp>
#include <unistd.h>
#include <fstream>

#include <iostream>
#include <csignal>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <chimbuko/util/barrier.hpp>

namespace tl = thallium;
namespace json = nlohmann;

bool terminate = false;

void termSignalHandler( int signum ) {
  terminate = true;
}


int main(int argc, char** argv) {
  //Initialize provider engine
  tl::engine engine("na+sm", THALLIUM_SERVER_MODE);
  engine.enable_remote_shutdown();

  //Write address to file
  std::string addr = (std::string)engine.self();    
  {
    std::ofstream f("provider.address");
    f << addr;
  }

  //Initialize provider
  sonata::Provider provider(engine);

  std::cout << "Provider is running on " << addr << std::endl;

  //Create a thread to act as admin
  std::thread admin_thr([&](){
      //tl::engine adengine("na+sm", THALLIUM_CLIENT_MODE);
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
      
      signal(SIGTERM, termSignalHandler);  
      
      //Spin quietly until SIGTERM sent
      std::cout << "Admin waiting for SIGTERM" << std::endl;
      while(!terminate){
	std::this_thread::sleep_for(std::chrono::seconds(1));
      }
      
      std::cout << "Admin detaching database" << std::endl;
      admin.detachDatabase(addr, 0, "provdb");

      std::cout << "Admin shutting down server" << std::endl;
      engine.finalize();
      std::cout << "Admin thread finished" << std::endl;
    });

  //Meanwhile the main thread sits acting as provider
  std::cout << "Provider thread waiting for finalize" << std::endl;
  engine.wait_for_finalize();
  
  std::cout << "Provider thread cleaning up" << std::endl;
  admin_thr.join();

  return 0;
}
