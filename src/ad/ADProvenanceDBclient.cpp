#include<chimbuko/ad/ADProvenanceDBclient.hpp>

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


thallium::engine ADProvenanceDBclient::m_engine("ofi+tcp", THALLIUM_CLIENT_MODE);
AnomalousSendManager ADProvenanceDBclient::anom_send_man;


void ADProvenanceDBclient::connect(const std::string &addr){
  try{
    std::cout << "DB client connecting to " << addr << std::endl;
    m_database = m_client.open(addr, 0, "provdb");
    std::cout << "DB client opening anomaly collection" << std::endl;
    m_coll_anomalies = m_database.open("anomalies");
    std::cout << "DB client opening metadata collection" << std::endl;
    m_coll_metadata = m_database.open("metadata");
    m_is_connected = true;
    std::cout << "DB client connected successfully" << std::endl;
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

  size_t size = entries.size();
  std::vector<uint64_t> ids(size);
  std::vector<std::string> dump(size);
  for(int i=0;i<size;i++){
    if(!entries[i].is_object()) throw std::runtime_error("Array entries must be JSON objects");
    dump[i] = entries[i].dump();    
  }

  getCollection(type).store_multi(dump, ids.data()); 
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
  
  getCollection(type).store(entry.dump(), ids, sreq);
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

  getCollection(type).store_multi(dump, ids, sreq); 
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

  getCollection(type).store_multi(dump, ids, sreq); 
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

std::vector<std::string> ADProvenanceDBclient::retrieveAllData(const ProvenanceDataType type){
  std::vector<std::string> out;
  if(m_is_connected)
    getCollection(type).all(&out);
  return out;
}
    
#endif
  

