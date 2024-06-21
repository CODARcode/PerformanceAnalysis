#include<ProvDBtester.hpp>
#include<chimbuko/core/provdb/ProvDBengine.hpp>
#include<chimbuko/core/util/string.hpp>
#include<unistd.h>
#include<sys/types.h>

using namespace chimbuko;

ProvDBtester::ProvDBtester(int nshards, const ProvDBmoduleSetupCore &setup){
  ProvDBengine::setProtocol("na+sm", THALLIUM_SERVER_MODE);
  auto &engine = ProvDBengine::getEngine();
  glob_provider = new sonata::Provider(engine, 0); 
  admin = new sonata::Admin(engine);
    
  addr = (std::string)engine.self();

  pid_t pid = getpid();

  std::string glob_db_name = "provdb.global";
  std::string glob_db_config = stringize("{ \"path\" : \"/tmp/%s.%lu.unqlite\" }", glob_db_name.c_str(),pid);
  admin->createDatabase(addr, 0, glob_db_name, "unqlite", glob_db_config);
	    
  db_shard_names.resize(nshards);
  shard_providers.resize(nshards);
  for(int s=0;s<nshards;s++){
    shard_providers[s] = new sonata::Provider(engine, s+1); 
    std::string db_name = stringize("provdb.%d",s);
    std::string config = stringize("{ \"path\" : \"/tmp/%s.%lu.unqlite\" }", db_name.c_str(),pid);
    admin->createDatabase(addr, s+1, db_name, "unqlite", config);
    db_shard_names[s] = db_name;
  }

  client = new sonata::Client(engine);
	  
  db.resize(nshards);
  for(int s=0;s<nshards;s++){
    db[s] = client->open(addr, s+1, db_shard_names[s]);
    auto const &colls = setup.getMainDBcollections();
    for(auto const &c : colls)  db[s].create(c);
  }
    
  glob_db = client->open(addr, 0, glob_db_name);
  auto const &colls = setup.getGlobalDBcollections();
  for(auto const &c : colls)  glob_db.create(c);
}


ProvDBtester::~ProvDBtester(){
  delete client;
  delete admin;
  delete glob_provider;
  for(int i=0;i<shard_providers.size();i++) delete shard_providers[i];
}
