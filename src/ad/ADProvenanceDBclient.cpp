#include<chimbuko/ad/ADProvenanceDBclient.hpp>
#include<chimbuko/verbose.hpp>

#ifdef ENABLE_PROVDB

using namespace chimbuko;


void AnomalousSendManager::purge(){
  if(outstanding.size() > MAX_OUTSTANDING){
    while(!outstanding.empty() && outstanding.front().completed()){ //requests completing in-order so we can stop when we encounter the first non-complete
      outstanding.front().wait(); //simply cleans up resources, non-blocking because already complete
      outstanding.pop();
    }
  }
}

sonata::AsyncRequest & AnomalousSendManager::getNewRequest(){
  purge();
  outstanding.emplace();
  return outstanding.back();
}

void AnomalousSendManager::waitAll(){
  while(!outstanding.empty()){ //flush the queue
    outstanding.front().wait();
    outstanding.pop();
  }
}  

AnomalousSendManager::~AnomalousSendManager(){
  waitAll();
}


AnomalousSendManager ADProvenanceDBclient::anom_send_man;


ADProvenanceDBclient::~ADProvenanceDBclient(){ 
  disconnect();
  VERBOSE(std::cout << "ADProvenanceDBclient exiting" << std::endl);
}

sonata::Collection & ADProvenanceDBclient::getCollection(const ProvenanceDataType type){ 
  switch(type){
  case ProvenanceDataType::AnomalyData:
    return m_coll_anomalies;
  case ProvenanceDataType::Metadata:
    return m_coll_metadata;
  case ProvenanceDataType::NormalExecData:
    return m_coll_normalexecs;
  default:
    throw std::runtime_error("Invalid type");
  }
}

const sonata::Collection & ADProvenanceDBclient::getCollection(const ProvenanceDataType type) const{ 
  return const_cast<ADProvenanceDBclient*>(this)->getCollection(type);
}



void ADProvenanceDBclient::disconnect(){
  if(m_is_connected){
    VERBOSE(std::cout << "ADProvenanceDBclient disconnecting" << std::endl);
    VERBOSE(std::cout << "ADProvenanceDBclient is waiting for outstanding calls to complete" << std::endl);
    anom_send_man.waitAll();
    VERBOSE(std::cout << "ADProvenanceDBclient de-registering with server" << std::endl);
    m_client_goodbye->on(m_server)(m_rank);    
    VERBOSE(std::cout << "ADProvenanceDBclient disconnected" << std::endl);

    delete m_client_hello; m_client_hello = nullptr;
    delete m_client_goodbye; m_client_goodbye = nullptr;
    m_is_connected = false;
  }
}

void ADProvenanceDBclient::connect(const std::string &addr){
  try{
    //Get the protocol
    size_t pos = addr.find(':');
    if(pos == std::string::npos) throw std::runtime_error("Address \"" + addr + "\" does not have expected form");
    std::string protocol = addr.substr(0,pos);
    //Ignore opening quotation marks
    while(protocol[0] == '"' || protocol[0] == '\'') protocol = protocol.substr(1,std::string::npos);
    
    if(ADProvenanceDBengine::getProtocol().first != protocol){
      int mode = ADProvenanceDBengine::getProtocol().second;
      VERBOSE(std::cout << "DB client reinitializing engine with protocol \"" << protocol << "\"" << std::endl);
      ADProvenanceDBengine::finalize();
      ADProvenanceDBengine::setProtocol(protocol,mode);      
    }      

    thallium::engine &eng = ADProvenanceDBengine::getEngine();
    m_client = sonata::Client(eng);
    VERBOSE(std::cout << "DB client connecting to " << addr << std::endl);
    m_database = m_client.open(addr, 0, "provdb");
    VERBOSE(std::cout << "DB client opening anomaly collection" << std::endl);
    m_coll_anomalies = m_database.open("anomalies");
    VERBOSE(std::cout << "DB client opening metadata collection" << std::endl);
    m_coll_metadata = m_database.open("metadata");
    VERBOSE(std::cout << "DB client opening normal execution collection" << std::endl);
    m_coll_normalexecs = m_database.open("normalexecs");


    VERBOSE(std::cout << "DB client registering RPCs and handshaking with provDB" << std::endl);
    m_server = eng.lookup(addr);
    m_client_hello = new thallium::remote_procedure(eng.define("client_hello").disable_response());
    m_client_goodbye = new thallium::remote_procedure(eng.define("client_goodbye").disable_response());
    m_client_hello->on(m_server)(m_rank);

    m_is_connected = true;
    VERBOSE(std::cout << "DB client connected successfully" << std::endl);
    
  }catch(const sonata::Exception& ex) {
    throw std::runtime_error(std::string("Provenance DB client could not connect due to exception: ") + ex.what());
  }
}

uint64_t ADProvenanceDBclient::sendData(const nlohmann::json &entry, const ProvenanceDataType type) const{
  if(!entry.is_object()) throw std::runtime_error("JSON entry must be an object");
  if(m_is_connected){
    return getCollection(type).store(entry.dump());
  }
  return -1;
}


std::vector<uint64_t> ADProvenanceDBclient::sendMultipleData(const std::vector<nlohmann::json> &entries, const ProvenanceDataType type) const{
  if(entries.size() == 0 || !m_is_connected) 
    return std::vector<uint64_t>();

  PerfTimer timer;

  timer.start();
  size_t size = entries.size();
  std::vector<uint64_t> ids(size);
  std::vector<std::string> dump(size);
  for(int i=0;i<size;i++){
    if(!entries[i].is_object()) throw std::runtime_error("Array entries must be JSON objects");
    dump[i] = entries[i].dump();    
  }
  if(m_stats){
    m_stats->add("provdb_client_sendmulti_dump_json_ms", timer.elapsed_ms());
    m_stats->add("provdb_client_sendmulti_nrecords", size);
    for(int i=0;i<size;i++) m_stats->add("provdb_client_sendmulti_record_size", dump[i].size());
  }

  timer.start();
  getCollection(type).store_multi(dump, ids.data()); 
  if(m_stats) m_stats->add("provdb_client_sendmulti_send_ms", timer.elapsed_ms());

  return ids;
}

std::vector<uint64_t> ADProvenanceDBclient::sendMultipleData(const nlohmann::json &entries, const ProvenanceDataType type) const{
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
  

void ADProvenanceDBclient::sendDataAsync(const nlohmann::json &entry, const ProvenanceDataType type, OutstandingRequest *req) const{
  if(!entry.is_object()) throw std::runtime_error("JSON entry must be an object");
  if(!m_is_connected) return;

  uint64_t* ids;
  sonata::AsyncRequest *sreq;

  if(req == nullptr){
    ids = nullptr; //utilize sonata mechanism for anomalous request
    sreq = &anom_send_man.getNewRequest();
  }else{
    req->ids.resize(1);
    ids = req->ids.data();
    sreq = &req->req;
  }
  
  getCollection(type).store(entry.dump(), ids, false, sreq);
}

void ADProvenanceDBclient::sendMultipleDataAsync(const std::vector<nlohmann::json> &entries, const ProvenanceDataType type, OutstandingRequest *req) const{
  if(entries.size() == 0 || !m_is_connected) return;

  size_t size = entries.size();
  std::vector<std::string> dump(size);
  for(int i=0;i<size;i++){
    if(!entries[i].is_object()) throw std::runtime_error("Array entries must be JSON objects");
    dump[i] = entries[i].dump();    
  }

  uint64_t* ids;
  sonata::AsyncRequest *sreq;

  if(req == nullptr){
    ids = nullptr; //utilize sonata mechanism for anomalous request
    sreq = &anom_send_man.getNewRequest();
  }else{
    req->ids.resize(size);
    ids = req->ids.data();
    sreq = &req->req;
  }

  getCollection(type).store_multi(dump, ids, false, sreq); 
}

void ADProvenanceDBclient::sendMultipleDataAsync(const nlohmann::json &entries, const ProvenanceDataType type, OutstandingRequest *req) const{
  if(!entries.is_array()) throw std::runtime_error("JSON object must be an array");
  size_t size = entries.size();
  
  if(size == 0 || !m_is_connected) 
    return;

  std::vector<std::string> dump(size);
  for(int i=0;i<size;i++){
    if(!entries[i].is_object()) throw std::runtime_error("Array entries must be JSON objects");
    dump[i] = entries[i].dump();
  }

  uint64_t* ids;
  sonata::AsyncRequest *sreq;

  if(req == nullptr){
    ids = nullptr; //utilize sonata mechanism for anomalous request
    sreq = &anom_send_man.getNewRequest();
  }else{
    req->ids.resize(size);
    ids = req->ids.data();
    sreq = &req->req;
  }

  getCollection(type).store_multi(dump, ids, false, sreq); 
}  






bool ADProvenanceDBclient::retrieveData(nlohmann::json &entry, uint64_t index, const ProvenanceDataType type) const{
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

void ADProvenanceDBclient::waitForSendComplete(){
  if(m_is_connected)
    anom_send_man.waitAll();
}

std::vector<std::string> ADProvenanceDBclient::retrieveAllData(const ProvenanceDataType type) const{
  std::vector<std::string> out;
  if(m_is_connected)
    getCollection(type).all(&out);
  return out;
}

std::vector<std::string> ADProvenanceDBclient::filterData(const ProvenanceDataType type, const std::string &query) const{
  std::vector<std::string> out;
  if(m_is_connected)
    getCollection(type).filter(query, &out);
  return out;
}

std::unordered_map<std::string,std::string> ADProvenanceDBclient::execute(const std::string &code, const std::unordered_set<std::string>& vars) const{
  std::unordered_map<std::string,std::string> out;
  if(m_is_connected)
    m_database.execute(code, vars, &out);
  return out;
}

    
#endif
  

