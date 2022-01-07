#include<chimbuko/pserver/PSProvenanceDBclient.hpp>
#include<chimbuko/verbose.hpp>
#include<chimbuko/util/string.hpp>
#include<chimbuko/provdb/setup.hpp>

#ifdef ENABLE_PROVDB

using namespace chimbuko;

PSProvenanceDBclient::PSProvenanceDBclient(): provDBclient(), m_client_hello(nullptr), m_client_goodbye(nullptr), m_perform_handshake(true){}

PSProvenanceDBclient::~PSProvenanceDBclient(){ 
  verboseStream << "PSProvenanceDBclient exiting" << std::endl;
  disconnect(); //call overridden disconnect explicitly even though base class also calls disconnect in destructor
}

yokan::Collection & PSProvenanceDBclient::getCollection(const GlobalProvenanceDataType type){ 
  switch(type){
  case GlobalProvenanceDataType::FunctionStats:
    return *m_coll_funcstats;
  case GlobalProvenanceDataType::CounterStats:
    return *m_coll_counterstats;
  default:
    throw std::runtime_error("Invalid type");
  }
}

const yokan::Collection & PSProvenanceDBclient::getCollection(const GlobalProvenanceDataType type) const{ 
  return const_cast<PSProvenanceDBclient*>(this)->getCollection(type);
}

void PSProvenanceDBclient::linkPerf(PerfStats *stats){ this->provDBclient::linkPerf(stats,"ps_provdb_client_"); }

void PSProvenanceDBclient::disconnect(){
  if(m_is_connected){
    verboseStream << "PSProvenanceDBclient disconnecting" << std::endl;

    if(m_perform_handshake){
      verboseStream << "PSProvenanceDBclient de-registering with server" << std::endl;
      m_client_goodbye->on(m_server)();    
      verboseStream << "PSProvenanceDBclient deleting handshake RPCs" << std::endl;

      m_client_hello->deregister();
      m_client_goodbye->deregister();
    
      delete m_client_hello; m_client_hello = nullptr;
      delete m_client_goodbye; m_client_goodbye = nullptr;
    }
    this->provDBclient::disconnect();
    verboseStream << "PSProvenanceDBclient disconnected" << std::endl;
  }
}


void PSProvenanceDBclient::connect(const std::string &addr, const int provider_idx){
  std::string db_name = ProvDBsetup::getGlobalDBname();
  this->provDBclient::connect(addr, db_name, provider_idx);

  try{    
    verboseStream << "PSProvenanceDBclient opening function stats collection" << std::endl;
    m_coll_funcstats.reset( openCollection(m_database,"func_stats") );
    verboseStream << "PSProvenanceDBclient opening counter stats collection" << std::endl;
    m_coll_counterstats.reset( openCollection(m_database,"counter_stats") );
    
    if(m_perform_handshake){
      verboseStream << "PSProvenanceDBclient registering RPCs and handshaking with provDB" << std::endl;
      m_client_hello = new thallium::remote_procedure(m_eng->define("pserver_hello").disable_response());
      m_client_goodbye = new thallium::remote_procedure(m_eng->define("pserver_goodbye").disable_response());
      m_client_hello->on(m_server)();
    }
    verboseStream << "PSProvenanceDBclient connected successfully" << std::endl;
  }catch(const yokan::Exception& ex) {
    m_is_connected = false;
    throw std::runtime_error(std::string("PSProvenanceDBclient could not connect due to exception: ") + ex.what());
  }
}

void PSProvenanceDBclient::connectMultiServer(const std::string &addr_file_dir){
  int instance = ProvDBsetup::getGlobalDBinstance();
  int provider = ProvDBsetup::getGlobalDBproviderIndex();
  std::string addr = ProvDBsetup::getInstanceAddress(instance, addr_file_dir);
  connect(addr, provider);
}

yk_id_t PSProvenanceDBclient::sendData(const nlohmann::json &entry, const GlobalProvenanceDataType type) const{
  return this->provDBclient::sendData(entry, getCollection(type));
}

std::vector<yk_id_t> PSProvenanceDBclient::sendMultipleData(const nlohmann::json &entries, const GlobalProvenanceDataType type) const{
  return this->provDBclient::sendMultipleData(entries, getCollection(type));
}

bool PSProvenanceDBclient::retrieveData(nlohmann::json &entry, yk_id_t index, const GlobalProvenanceDataType type) const{
  return this->provDBclient::retrieveData(entry, index, getCollection(type));
}

std::vector<std::string> PSProvenanceDBclient::retrieveAllData(const GlobalProvenanceDataType type) const{
  return this->provDBclient::retrieveAllData(getCollection(type));
}

std::vector<std::string> PSProvenanceDBclient::filterData(const GlobalProvenanceDataType type, const std::string &query) const{
  return this->provDBclient::filterData(getCollection(type), query);
}

    
#endif
  

