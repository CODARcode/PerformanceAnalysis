#include<chimbuko/pserver/PSProvenanceDBclient.hpp>
#include<chimbuko/verbose.hpp>
#include<chimbuko/util/string.hpp>
#include<chimbuko/provdb/setup.hpp>

#ifdef ENABLE_PROVDB

using namespace chimbuko;

PSProvenanceDBclient::~PSProvenanceDBclient(){ 
  disconnect();
  verboseStream << "PSProvenanceDBclient exiting" << std::endl;
}

sonata::Collection & PSProvenanceDBclient::getCollection(const GlobalProvenanceDataType type){ 
  switch(type){
  case GlobalProvenanceDataType::FunctionStats:
    return m_coll_funcstats;
  case GlobalProvenanceDataType::CounterStats:
    return m_coll_counterstats;
  case GlobalProvenanceDataType::ADModel:
    return m_coll_admodel;
  default:
    throw std::runtime_error("Invalid type");
  }
}

const sonata::Collection & PSProvenanceDBclient::getCollection(const GlobalProvenanceDataType type) const{ 
  return const_cast<PSProvenanceDBclient*>(this)->getCollection(type);
}



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

    m_is_connected = false;
    verboseStream << "PSProvenanceDBclient disconnected" << std::endl;
  }
}

void PSProvenanceDBclient::connect(const std::string &addr, const int provider_idx){
  try{    
    //Reset the protocol if necessary
    std::string protocol = ADProvenanceDBengine::getProtocolFromAddress(addr);   
    if(ADProvenanceDBengine::getProtocol().first != protocol){
      int mode = ADProvenanceDBengine::getProtocol().second;
      verboseStream << "PSProvenanceDBclient reinitializing engine with protocol \"" << protocol << "\"" << std::endl;
      ADProvenanceDBengine::finalize();
      ADProvenanceDBengine::setProtocol(protocol,mode);      
    }      

    std::string db_name = ProvDBsetup::getGlobalDBname();

    thallium::engine &eng = ADProvenanceDBengine::getEngine();
    m_client = sonata::Client(eng);
    verboseStream << "PSProvenanceDBclient connecting to database " << db_name << " on address " << addr << std::endl;
    m_database = m_client.open(addr, provider_idx, db_name);
    verboseStream << "PSProvenanceDBclient opening function stats collection" << std::endl;
    m_coll_funcstats = m_database.open("func_stats");
    verboseStream << "PSProvenanceDBclient opening counter stats collection" << std::endl;
    m_coll_counterstats = m_database.open("counter_stats");
    verboseStream << "PSProvenanceDBclient opening AD model collection" << std::endl;
    m_coll_admodel = m_database.open("ad_model");

    
    m_server = eng.lookup(addr);

    if(m_perform_handshake){
      verboseStream << "PSProvenanceDBclient registering RPCs and handshaking with provDB" << std::endl;
      m_client_hello = new thallium::remote_procedure(eng.define("pserver_hello").disable_response());
      m_client_goodbye = new thallium::remote_procedure(eng.define("pserver_goodbye").disable_response());
      m_client_hello->on(m_server)();
    }      

    m_is_connected = true;
    verboseStream << "PSProvenanceDBclient connected successfully" << std::endl;
    
  }catch(const sonata::Exception& ex) {
    throw std::runtime_error(std::string("PSProvenanceDBclient could not connect due to exception: ") + ex.what());
  }
}

void PSProvenanceDBclient::connectMultiServer(const std::string &addr_file_dir){
  int instance = ProvDBsetup::getGlobalDBinstance();
  int provider = ProvDBsetup::getGlobalDBproviderIndex();
  std::string addr = ProvDBsetup::getInstanceAddress(instance, addr_file_dir);
  connect(addr, provider);
}

uint64_t PSProvenanceDBclient::sendData(const nlohmann::json &entry, const GlobalProvenanceDataType type) const{
  if(!entry.is_object()) throw std::runtime_error("JSON entry must be an object");
  if(m_is_connected){
    return getCollection(type).store(entry.dump());
  }
  return -1;
}

std::vector<uint64_t> PSProvenanceDBclient::sendMultipleData(const nlohmann::json &entries, const GlobalProvenanceDataType type) const{
  if(!entries.is_array()) throw std::runtime_error("JSON object must be an array");
  size_t size = entries.size();
  
  if(size == 0 || !m_is_connected) 
    return std::vector<uint64_t>();

  std::vector<uint64_t> ids(size,-1);
  std::vector<std::string> dump(size);
  for(int i=0;i<size;i++){
    if(!entries[i].is_object()) throw std::runtime_error("Array entries must be JSON objects");
    dump[i] = entries[i].dump();
  }

  getCollection(type).store_multi(dump, ids.data()); 
  return ids;
}  


bool PSProvenanceDBclient::retrieveData(nlohmann::json &entry, uint64_t index, const GlobalProvenanceDataType type) const{
  if(m_is_connected){
    std::string data;
    try{
      getCollection(type).fetch(index, &data);
    }catch(const sonata::Exception &e){
      return false;
    }
    entry = nlohmann::json::parse(data);
    return true;
  }
  return false;
}

std::vector<std::string> PSProvenanceDBclient::retrieveAllData(const GlobalProvenanceDataType type) const{
  std::vector<std::string> out;
  if(m_is_connected)
    getCollection(type).all(&out);
  return out;
}

std::vector<std::string> PSProvenanceDBclient::filterData(const GlobalProvenanceDataType type, const std::string &query) const{
  std::vector<std::string> out;
  if(m_is_connected)
    getCollection(type).filter(query, &out);
  return out;
}

    
#endif
  

