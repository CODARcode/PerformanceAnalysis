#include<sim/provdb.hpp>
#ifdef ENABLE_PROVDB
#include<chimbuko/util/string.hpp>


using namespace chimbuko;
using namespace chimbuko_sim;

provDBsim::provDBsim(int nshards){
  ADProvenanceDBengine::setProtocol("na+sm", THALLIUM_SERVER_MODE);
  auto &engine = ADProvenanceDBengine::getEngine();
  provider = new sonata::Provider(engine, 0);
  admin = new sonata::Admin(engine);
    
  addr = (std::string)engine.self();

  std::string glob_db_name = "provdb.global";
  std::string glob_db_config = stringize("{ \"path\" : \"./%s.unqlite\" }", glob_db_name.c_str());
  admin->createDatabase(addr, 0, glob_db_name, "unqlite", glob_db_config);
	    
  db_shard_names.resize(nshards);
  for(int s=0;s<nshards;s++){
    std::string db_name = stringize("provdb.%d",s);
    std::string config = stringize("{ \"path\" : \"./%s.unqlite\" }", db_name.c_str());
    admin->createDatabase(addr, 0, db_name, "unqlite", config);
    db_shard_names[s] = db_name;
  }

  client = new sonata::Client(engine);
	  
  db.resize(nshards);
  for(int s=0;s<nshards;s++){
    db[s] = client->open(addr, 0, db_shard_names[s]);
    db[s].create("anomalies");
    db[s].create("metadata");
    db[s].create("normalexecs");
  }
    
  glob_db = client->open(addr, 0, glob_db_name);
  glob_db.create("func_stats");
  glob_db.create("counter_stats");
}


provDBsim::~provDBsim(){
  delete client;
  delete admin;
  delete provider;
}

#endif
