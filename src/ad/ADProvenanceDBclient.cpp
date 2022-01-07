#include<atomic>
#include<chrono>
#include<thread>
#include<yokan/cxx/admin.hpp>
#include<chimbuko/ad/ADProvenanceDBclient.hpp>
#include<chimbuko/verbose.hpp>
#include<chimbuko/util/string.hpp>
#include<chimbuko/provdb/setup.hpp>
#include<chimbuko/util/error.hpp>

#ifdef ENABLE_PROVDB

using namespace chimbuko;

ADProvenanceDBclient::~ADProvenanceDBclient(){ 
  verboseStream << "ADProvenanceDBclient exiting" << std::endl;
  disconnect(); //call overridden disconnect explicitly even though base class also calls disconnect in destructor
}

yokan::Collection & ADProvenanceDBclient::getCollection(const ProvenanceDataType type){ 
  switch(type){
  case ProvenanceDataType::AnomalyData:
    return *m_coll_anomalies;
  case ProvenanceDataType::Metadata:
    return *m_coll_metadata;
  case ProvenanceDataType::NormalExecData:
    return *m_coll_normalexecs;
  default:
    throw std::runtime_error("Invalid type");
  }
}

const yokan::Collection & ADProvenanceDBclient::getCollection(const ProvenanceDataType type) const{ 
  return const_cast<ADProvenanceDBclient*>(this)->getCollection(type);
}

static void delete_rpc(thallium::remote_procedure* &rpc){
  delete rpc; rpc = nullptr;
}

ADProvenanceDBclient::ADProvenanceDBclient(int rank): provDBclient(), m_rank(rank), m_client_hello(nullptr), m_client_goodbye(nullptr), m_stop_server(nullptr), m_perform_handshake(true)
#ifdef ENABLE_MARGO_STATE_DUMP
    , m_margo_dump(nullptr)
#endif
    {}


void ADProvenanceDBclient::disconnect(){
  if(m_is_connected){
    verboseStream << "ADProvenanceDBclient disconnecting" << std::endl;
    verboseStream << "ADProvenanceDBclient is waiting for outstanding calls to complete" << std::endl;

    PerfTimer timer;
    anom_send_man.waitAll();
    if(m_stats) m_stats->add("provdb_client_disconnect_wait_all_ms", timer.elapsed_ms());

    if(m_perform_handshake){
      verboseStream << "ADProvenanceDBclient de-registering with server" << std::endl;
      m_client_goodbye->on(m_server)(m_rank);    
      verboseStream << "ADProvenanceDBclient deleting handshake RPCs" << std::endl;
      delete_rpc(m_client_hello);
      delete_rpc(m_client_goodbye);
    }
    delete_rpc(m_stop_server);
#ifdef ENABLE_MARGO_STATE_DUMP
    delete_rpc(m_margo_dump);
#endif

    this->provDBclient::disconnect();
    verboseStream << "ADProvenanceDBclient disconnected" << std::endl;
  }
}

void ADProvenanceDBclient::connect(const std::string &addr, const std::string &db_name, const int provider_idx){
  this->provDBclient::connect(addr,db_name,provider_idx);
  if(m_is_connected){ //Open collections and setup RPCs
    try{
      verboseStream << "DB client opening anomaly collection" << std::endl;
      m_coll_anomalies.reset( openCollection(m_database,"anomalies") );
      verboseStream << "DB client opening metadata collection" << std::endl;
      m_coll_metadata.reset( openCollection(m_database,"metadata") );
      verboseStream << "DB client opening normal execution collection" << std::endl;
      m_coll_normalexecs.reset( openCollection(m_database,"normalexecs") );

      if(m_perform_handshake){
	verboseStream << "DB client registering RPCs and handshaking with provDB" << std::endl;
	m_client_hello = new thallium::remote_procedure(m_eng->define("client_hello").disable_response());
	m_client_goodbye = new thallium::remote_procedure(m_eng->define("client_goodbye").disable_response());
	m_client_hello->on(m_server)(m_rank);
      }      

      m_stop_server = new thallium::remote_procedure(m_eng->define("stop_server").disable_response());
#ifdef ENABLE_MARGO_STATE_DUMP
      m_margo_dump = new thallium::remote_procedure(m_eng->define("margo_dump").disable_response());
#endif
      m_server_addr = addr;
      verboseStream << "DB client rank " << m_rank << " connected successfully to database" << std::endl;
    }catch(const yokan::Exception& ex) {
      m_is_connected = false;
      throw std::runtime_error(std::string("Provenance DB client could not connect due to exception: ") + ex.what());
    }
  }
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
  verboseStream << "DB client rank " << m_rank << " assigned shard " << shard << " on instance "<< instance << std::endl;
  std::string addr = setup.getInstanceAddress(instance, addr_file_dir);
  std::string db_name = setup.getShardDBname(shard);
  int provider = setup.getShardProviderIndex(shard);
  
  connect(addr, db_name, provider);
}

yk_id_t ADProvenanceDBclient::sendData(const nlohmann::json &entry, const ProvenanceDataType type) const{
  return this->provDBclient::sendData(entry, getCollection(type));
}

std::vector<yk_id_t> ADProvenanceDBclient::sendMultipleData(const std::vector<nlohmann::json> &entries, const ProvenanceDataType type) const{
  return this->provDBclient::sendMultipleData(entries, getCollection(type));
}

std::vector<yk_id_t> ADProvenanceDBclient::sendMultipleData(const nlohmann::json &entries, const ProvenanceDataType type) const{
  return this->provDBclient::sendMultipleData(entries, getCollection(type));
}
  

void ADProvenanceDBclient::sendDataAsync(const nlohmann::json &entry, const ProvenanceDataType type, OutstandingRequest *req) const{
  return this->provDBclient::sendDataAsync(entry, getCollection(type), req);
}


void ADProvenanceDBclient::sendMultipleDataAsync(const std::vector<nlohmann::json> &entries, const ProvenanceDataType type, OutstandingRequest *req) const{
  return this->provDBclient::sendMultipleDataAsync(entries, getCollection(type), req);
}

void ADProvenanceDBclient::sendMultipleDataAsync(const nlohmann::json &entries, const ProvenanceDataType type, OutstandingRequest *req) const{
  return this->provDBclient::sendMultipleDataAsync(entries, getCollection(type), req);
}  

void ADProvenanceDBclient::linkPerf(PerfStats *stats){ this->provDBclient::linkPerf(stats,"provdb_client_"); }





bool ADProvenanceDBclient::retrieveData(nlohmann::json &entry, yk_id_t index, const ProvenanceDataType type) const{
  return this->provDBclient::retrieveData(entry, index, getCollection(type));
}

void ADProvenanceDBclient::clearAllData(const ProvenanceDataType type) const{
  return this->provDBclient::clearAllData(getCollection(type));
}


std::vector<std::string> ADProvenanceDBclient::retrieveAllData(const ProvenanceDataType type) const{
  return this->provDBclient::retrieveAllData(getCollection(type));
}

std::vector<std::string> ADProvenanceDBclient::filterData(const ProvenanceDataType type, const std::string &query) const{
  return this->provDBclient::filterData(getCollection(type), query);
}



std::unordered_map<std::string,std::string> ADProvenanceDBclient::execute(const std::string &code, const std::unordered_set<std::string>& vars) const{
  assert(0);
  // std::unordered_map<std::string,std::string> out;
  // if(m_is_connected)
  //   m_database.execute(code, vars, &out);
  // return out;
}

void ADProvenanceDBclient::stopServer() const{
  verboseStream << "ADProvenanceDBclient stopping server" << std::endl;
  m_stop_server->on(m_server)();
}

#ifdef ENABLE_MARGO_STATE_DUMP
void ADProvenanceDBclient::serverDumpState() const{
  verboseStream << "ADProvenanceDBclient requesting server dump its state" << std::endl;
  m_margo_dump->on(m_server)();
}
#endif
    
#endif
  

