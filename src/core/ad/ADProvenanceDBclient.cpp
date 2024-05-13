#include<atomic>
#include<chrono>
#include<thread>
#include<chimbuko/core/ad/ADProvenanceDBclient.hpp>
#include<chimbuko/core/verbose.hpp>
#include<chimbuko/core/util/string.hpp>
#include<chimbuko/core/provdb/setup.hpp>
#include<chimbuko/core/util/error.hpp>

#ifdef ENABLE_PROVDB

using namespace chimbuko;

static void delete_rpc(thallium::remote_procedure* &rpc){
  delete rpc; rpc = nullptr;
}

ADProvenanceDBclient::ADProvenanceDBclient(const std::vector<std::string> &collection_names, int rank): m_rank(rank), m_client_hello(nullptr), m_client_goodbye(nullptr), ProvDBclient(collection_names, stringize("rank %d",rank)){}

ADProvenanceDBclient::~ADProvenanceDBclient(){ disconnect(); } //call disconnect in derived class to ensure derived class is still alive when disconnect is called

void ADProvenanceDBclient::handshakeHello(thallium::engine &eng, thallium::endpoint &server){
  verboseStream << "ADProvenanceDBclient rank " << m_rank << " registering RPCs and handshaking with provDB" << std::endl;
  m_client_hello = new thallium::remote_procedure(eng.define("client_hello").disable_response());
  m_client_goodbye = new thallium::remote_procedure(eng.define("client_goodbye").disable_response());
  m_client_hello->on(server)(m_rank);
}
void ADProvenanceDBclient::handshakeGoodbye(thallium::engine &eng, thallium::endpoint &server){
  verboseStream << "ADProvenanceDBclient rank " << m_rank << " de-registering with server" << std::endl;
  m_client_goodbye->on(server)(m_rank);    
  verboseStream << "ADProvenanceDBclient rank " << m_rank << " deleting handshake RPCs" << std::endl;
  delete_rpc(m_client_hello);
  delete_rpc(m_client_goodbye);
}

void ADProvenanceDBclient::connectSingleServer(const std::string &addr, const int nshards){
  ProvDBsetup setup(nshards, 1);

  //Assign shards round-robin
  int shard = m_rank % nshards;      
  std::string db_name = setup.getShardDBname(shard);
  int provider = setup.getShardProviderIndex(shard);
  
  connect(addr, db_name, provider);
}

void ADProvenanceDBclient::connectMultiServer(const std::string &addr_file_dir, const int nshards, const int ninstances){
  ProvDBsetup setup(nshards, ninstances);

  //Assign shards round-robin
  int shard = m_rank % nshards;
  int instance = setup.getShardInstance(shard); //which server instance
  std::string addr = setup.getInstanceAddress(instance, addr_file_dir);
  std::string db_name = setup.getShardDBname(shard);
  int provider = setup.getShardProviderIndex(shard);
  
  connect(addr, db_name, provider);
}

void ADProvenanceDBclient::connectMultiServerShardAssign(const std::string &addr_file_dir, const int nshards, const int ninstances, const std::string &shard_assign_file){
  ProvDBsetup setup(nshards, ninstances);

  std::ifstream f(shard_assign_file);
  if(f.fail()) throw std::runtime_error("Could not open shard assignment file " + shard_assign_file + "\n");
  int shard = -1;
  for(int i=0;i<=m_rank;i++){
    f >> shard;
    if(f.fail()) throw std::runtime_error("Failed to reach assignment for rank " + std::to_string(m_rank) + "\n");
  }
  f.close();

  int instance = setup.getShardInstance(shard); //which server instance
  verboseStream << "ADProvenanceDBclient rank " << m_rank << " assigned shard " << shard << " on instance "<< instance << std::endl;
  std::string addr = setup.getInstanceAddress(instance, addr_file_dir);
  std::string db_name = setup.getShardDBname(shard);
  int provider = setup.getShardProviderIndex(shard);
  
  connect(addr, db_name, provider);
}
    
#endif
  

