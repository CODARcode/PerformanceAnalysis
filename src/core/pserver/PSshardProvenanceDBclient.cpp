#include<chimbuko/core/pserver/PSshardProvenanceDBclient.hpp>
#include<chimbuko/core/verbose.hpp>
#include<chimbuko/core/util/string.hpp>

#ifdef ENABLE_PROVDB

using namespace chimbuko;

PSshardProvenanceDBclient::~PSshardProvenanceDBclient(){ disconnect(); } //call disconnect in derived class to ensure derived class is still alive when disconnect is called

void PSshardProvenanceDBclient::connectShard(const std::string &addr_file_dir, int shard, int nshards, int ninstances){
  ProvDBsetup setup(nshards, ninstances);

  int instance = setup.getShardInstance(shard);
  int provider = setup.getShardProviderIndex(shard);
  std::string db_name = setup.getShardDBname(shard);
  std::string addr = setup.getInstanceAddress(instance, addr_file_dir);
  connect(addr,db_name,provider);
}
    
#endif
  
