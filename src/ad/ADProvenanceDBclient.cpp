#include<atomic>
#include<chrono>
#include<thread>
#include<chimbuko/ad/ADProvenanceDBclient.hpp>
#include<chimbuko/verbose.hpp>
#include<chimbuko/util/string.hpp>


#ifdef ENABLE_PROVDB

using namespace chimbuko;


void AnomalousSendManager::purge(){
  while(!outstanding.empty() && outstanding.front().completed()){ //requests completing in-order so we can stop when we encounter the first non-complete
    outstanding.front().wait(); //simply cleans up resources, non-blocking because already complete
    outstanding.pop();
  }
}

sonata::AsyncRequest & AnomalousSendManager::getNewRequest(){
  purge();
  outstanding.emplace();
  return outstanding.back();
}

void AnomalousSendManager::waitAll(){
  //Have another thread produce heartbeat information so we can know if the AD gets stuck waiting to flush
  std::atomic<bool> ready(false); 
  int heartbeat_freq = 20;
  std::thread heartbeat([heartbeat_freq, &ready]{
			  typedef std::chrono::high_resolution_clock Clock;
			  typedef std::chrono::seconds Sec;
			  Clock::time_point start = Clock::now();
			  while(!ready.load(std::memory_order_relaxed)){
			    int sec = std::chrono::duration_cast<Sec>(Clock::now() - start).count();
			    if(sec >= heartbeat_freq && sec % heartbeat_freq == 0) //complain every heartbeat_freq seconds
			      std::cout << "AnomalousSendManager::waitAll still waiting for queue flush after " << sec << "s" << std::endl;
			    std::this_thread::sleep_for(std::chrono::seconds(1));
			  }
			});	    
  
  while(!outstanding.empty()){ //flush the queue
    outstanding.front().wait();
    outstanding.pop();
  }
  ready.store(true, std::memory_order_relaxed);
  heartbeat.join();
}  

size_t AnomalousSendManager::getNoutstanding(){
  purge();
  return outstanding.size();
}

AnomalousSendManager::~AnomalousSendManager(){
  waitAll();
  verboseStream << "AnomalousSendManager exiting" << std::endl;
}


AnomalousSendManager ADProvenanceDBclient::anom_send_man;


ADProvenanceDBclient::~ADProvenanceDBclient(){ 
  disconnect();
  verboseStream << "ADProvenanceDBclient exiting" << std::endl;
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

static void delete_rpc(thallium::remote_procedure* &rpc){
  rpc->deregister();
  delete rpc; rpc = nullptr;
}

ADProvenanceDBclient::ADProvenanceDBclient(int rank): m_is_connected(false), m_rank(rank), m_client_hello(nullptr), m_client_goodbye(nullptr), m_stats(nullptr), m_stop_server(nullptr), m_perform_handshake(true)
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
    m_is_connected = false;
    verboseStream << "ADProvenanceDBclient disconnected" << std::endl;
  }
}

void ADProvenanceDBclient::connect(const std::string &addr, const int nshards){
  try{
    //Reset the protocol if necessary
    std::string protocol = ADProvenanceDBengine::getProtocolFromAddress(addr);
    if(ADProvenanceDBengine::getProtocol().first != protocol){
      int mode = ADProvenanceDBengine::getProtocol().second;
      verboseStream << "DB client reinitializing engine with protocol \"" << protocol << "\"" << std::endl;
      ADProvenanceDBengine::finalize();
      ADProvenanceDBengine::setProtocol(protocol,mode);      
    }      

    int shard = m_rank % nshards;
    std::string db_name = stringize("provdb.%d", shard);

    thallium::engine &eng = ADProvenanceDBengine::getEngine();
    m_client = sonata::Client(eng);
    verboseStream << "DB client rank " << m_rank << " connecting to database " << db_name << " on address " << addr << std::endl;
    
    {
      //Have another thread produce heartbeat information so we can know if the AD gets stuck waiting to connect to the provDB
      std::atomic<bool> ready(false); 
      int rank = m_rank;
      int heartbeat_freq = 20;
      std::thread heartbeat([heartbeat_freq, rank, &ready]{
	  typedef std::chrono::high_resolution_clock Clock;
	  typedef std::chrono::seconds Sec;
	  Clock::time_point start = Clock::now();
	  while(!ready.load(std::memory_order_relaxed)){
	    int sec = std::chrono::duration_cast<Sec>(Clock::now() - start).count();
	    if(sec >= heartbeat_freq && sec % heartbeat_freq == 0) //complain every heartbeat_freq seconds
	      std::cout << "DB client rank " << rank << " still waiting for provDB connection after " << sec << "s" << std::endl;
	    std::this_thread::sleep_for(std::chrono::seconds(1));
	  }
	});	    

      m_database = m_client.open(addr, 0, db_name);
      ready.store(true, std::memory_order_relaxed);
      heartbeat.join();
    }
    verboseStream << "DB client opening anomaly collection" << std::endl;
    m_coll_anomalies = m_database.open("anomalies");
    verboseStream << "DB client opening metadata collection" << std::endl;
    m_coll_metadata = m_database.open("metadata");
    verboseStream << "DB client opening normal execution collection" << std::endl;
    m_coll_normalexecs = m_database.open("normalexecs");

    m_server = eng.lookup(addr);

    if(m_perform_handshake){
      verboseStream << "DB client registering RPCs and handshaking with provDB" << std::endl;
      m_client_hello = new thallium::remote_procedure(eng.define("client_hello").disable_response());
      m_client_goodbye = new thallium::remote_procedure(eng.define("client_goodbye").disable_response());
      m_client_hello->on(m_server)(m_rank);
    }      

    m_stop_server = new thallium::remote_procedure(eng.define("stop_server").disable_response());
#ifdef ENABLE_MARGO_STATE_DUMP
    m_margo_dump = new thallium::remote_procedure(eng.define("margo_dump").disable_response());
#endif

    m_is_connected = true;
    verboseStream << "DB client rank " << m_rank << " connected successfully to database" << std::endl;

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

void ADProvenanceDBclient::sendMultipleDataAsync(const std::vector<std::string> &entries, const ProvenanceDataType type, OutstandingRequest *req) const{
  if(entries.size() == 0 || !m_is_connected) return;

  PerfTimer timer, ftimer;
  size_t size = entries.size();
  uint64_t* ids;
  sonata::AsyncRequest *sreq;

  if(req == nullptr){
    ids = nullptr; //utilize sonata mechanism for anomalous request
    ftimer.start();
    sreq = &anom_send_man.getNewRequest();
    if(m_stats) m_stats->add("provdb_client_sendmulti_async_getnewreq_ms", timer.elapsed_ms());
  }else{
    req->ids.resize(size);
    ids = req->ids.data();
    sreq = &req->req;
  }

  getCollection(type).store_multi(entries, ids, false, sreq); 

  if(m_stats) m_stats->add("provdb_client_sendmulti_async_send_ms", timer.elapsed_ms());
}



void ADProvenanceDBclient::sendMultipleDataAsync(const std::vector<nlohmann::json> &entries, const ProvenanceDataType type, OutstandingRequest *req) const{
  if(entries.size() == 0 || !m_is_connected) return;

  PerfTimer timer;
  size_t size = entries.size();
  std::vector<std::string> dump(size);
  for(int i=0;i<size;i++){
    if(!entries[i].is_object()) throw std::runtime_error("Array entries must be JSON objects");
    dump[i] = entries[i].dump();    
  }

  if(m_stats){
    m_stats->add("provdb_client_sendmulti_async_dump_json_ms", timer.elapsed_ms());
    m_stats->add("provdb_client_sendmulti_async_nrecords", size);
    for(int i=0;i<size;i++) m_stats->add("provdb_client_sendmulti_async_record_size", dump[i].size());
  }

  sendMultipleDataAsync(dump, type, req);
}

void ADProvenanceDBclient::sendMultipleDataAsync(const nlohmann::json &entries, const ProvenanceDataType type, OutstandingRequest *req) const{
  if(!entries.is_array()) throw std::runtime_error("JSON object must be an array");
  size_t size = entries.size();
  
  if(size == 0 || !m_is_connected) 
    return;

  PerfTimer timer;
  std::vector<std::string> dump(size);
  for(int i=0;i<size;i++){
    if(!entries[i].is_object()) throw std::runtime_error("Array entries must be JSON objects");
    dump[i] = entries[i].dump();
  }

  if(m_stats){
    m_stats->add("provdb_client_sendmulti_async_dump_json_ms", timer.elapsed_ms());
    m_stats->add("provdb_client_sendmulti_async_nrecords", size);
    for(int i=0;i<size;i++) m_stats->add("provdb_client_sendmulti_async_record_size", dump[i].size());
  }

  sendMultipleDataAsync(dump, type, req);
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
  

