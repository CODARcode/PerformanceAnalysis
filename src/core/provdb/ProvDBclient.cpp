#include<atomic>
#include<chrono>
#include<thread>
#include<chimbuko/core/provdb/ProvDBclient.hpp>
#include<chimbuko/core/verbose.hpp>
#include<chimbuko/core/util/string.hpp>
#include<chimbuko/core/provdb/setup.hpp>
#include<chimbuko/core/util/error.hpp>

#ifdef ENABLE_PROVDB

using namespace chimbuko;


void AnomalousSendManager::purge(){
  auto it = outstanding.begin();
  while(it != outstanding.end()){
    if(it->completed()) 
      it = outstanding.erase(it);
    else 
      ++it;
  }

  int nremaining = outstanding.size();
  int ncomplete_remaining = 0;
  for(auto const &e: outstanding)
    if(e.completed()) ++ncomplete_remaining;
  
  if(ncomplete_remaining > 0)
    recoverable_error("After purging complete sends from the front of the queue, the queue still contains " + std::to_string(ncomplete_remaining) + "/" + std::to_string(nremaining) + " completed but unremoved ids");
}

sonata::AsyncRequest & AnomalousSendManager::getNewRequest(){
  purge();
  outstanding.emplace_back();
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
    outstanding.pop_front();
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

ProvDBclient::~ProvDBclient(){}

sonata::Collection & ProvDBclient::getCollection(const std::string &name){ 
  auto it = m_collections.find(name);
  if(it == m_collections.end()) fatal_error("Unknown collection "+name);
  return it->second;
}

const sonata::Collection & ProvDBclient::getCollection(const std::string &collection) const{ 
  return const_cast<ProvDBclient*>(this)->getCollection(collection);
}

static void delete_rpc(thallium::remote_procedure* &rpc){
  delete rpc; rpc = nullptr;
}

ProvDBclient::ProvDBclient(const std::vector<std::string> &collection_names, const std::string &instance_descr): m_is_connected(false), m_stats(nullptr), m_stop_server(nullptr), m_perform_handshake(true), m_collection_names(collection_names), m_instance_descr(instance_descr)
#ifdef ENABLE_MARGO_STATE_DUMP
    , m_margo_dump(nullptr)
#endif
    {}


void ProvDBclient::disconnect(){
  if(m_is_connected){
    verboseStream << "ProvDBclient " << m_instance_descr << " disconnecting" << std::endl;
    verboseStream << "ProvDBclient " << m_instance_descr << " is waiting for outstanding calls to complete" << std::endl;

    PerfTimer timer;
    anom_send_man.waitAll();
    if(m_stats) m_stats->add("provdb_client_disconnect_wait_all_ms", timer.elapsed_ms());

    if(m_perform_handshake){
      this->handshakeGoodbye(ProvDBengine::getEngine(),m_server);
    }
      
    delete_rpc(m_stop_server);
#ifdef ENABLE_MARGO_STATE_DUMP
    delete_rpc(m_margo_dump);
#endif
    m_is_connected = false;
    verboseStream << "ProvDBclient " << m_instance_descr << " disconnected" << std::endl;
  }
}

void ProvDBclient::connect(const std::string &addr, const std::string &db_name, const int provider_idx){
  try{
    //Reset the protocol if necessary
    std::string protocol = ProvDBengine::getProtocolFromAddress(addr);
    if(ProvDBengine::getProtocol().first != protocol){
      int mode = ProvDBengine::getProtocol().second;
      verboseStream << "ProvDBclient " << m_instance_descr << " reinitializing engine with protocol \"" << protocol << "\"" << std::endl;
      ProvDBengine::finalize();
      ProvDBengine::setProtocol(protocol,mode);      
    }      

    thallium::engine &eng = ProvDBengine::getEngine();
    m_client = sonata::Client(eng);
    verboseStream << "ProvDBclient " << m_instance_descr << " connecting to database " << db_name << " on address " << addr << " and provider idx " << provider_idx << std::endl;
    
    {
      //Have another thread produce heartbeat information so we can know if the AD gets stuck waiting to connect to the provDB
      std::atomic<bool> ready(false); 
      int rank = m_rank;
      int heartbeat_freq = 20;
      std::string inst_descr = m_instance_descr;
      std::thread heartbeat([heartbeat_freq, rank, &ready, inst_descr]{
	  typedef std::chrono::high_resolution_clock Clock;
	  typedef std::chrono::seconds Sec;
	  Clock::time_point start = Clock::now();
	  while(!ready.load(std::memory_order_relaxed)){
	    int sec = std::chrono::duration_cast<Sec>(Clock::now() - start).count();
	    if(sec >= heartbeat_freq && sec % heartbeat_freq == 0) //complain every heartbeat_freq seconds
	      std::cout << "provDBclient " << inst_descr << " still waiting for provDB connection after " << sec << "s" << std::endl;
	    std::this_thread::sleep_for(std::chrono::seconds(1));
	  }
	});	    

      m_database = m_client.open(addr, provider_idx, db_name);
      ready.store(true, std::memory_order_relaxed);
      heartbeat.join();
    }
    for(auto const &c : m_collection_names){
      verboseStream << "ProvDBclient " << m_instance_descr << " opening '" << c << "' collection" << std::endl;
      m_collections[c] = m_database.open(c);
    }

    m_server = eng.lookup(addr);

    if(m_perform_handshake){
      this->handshakeHello(eng,m_server);
    }      

    m_stop_server = new thallium::remote_procedure(eng.define("stop_server").disable_response());
#ifdef ENABLE_MARGO_STATE_DUMP
    m_margo_dump = new thallium::remote_procedure(eng.define("margo_dump").disable_response());
#endif
    m_server_addr = addr;
    m_is_connected = true;
    verboseStream << "ProvDBclient " << m_instance_descr << " connected successfully to database" << std::endl;

  }catch(const sonata::Exception& ex) {
    throw std::runtime_error(std::string("Provenance DB client could not connect due to exception: ") + ex.what());
  }
}

uint64_t ProvDBclient::sendData(const nlohmann::json &entry, const std::string &collection) const{
  if(!entry.is_object()) throw std::runtime_error("JSON entry must be an object");
  if(m_is_connected){
    return getCollection(collection).store(entry.dump());
  }
  return -1;
}


std::vector<uint64_t> ProvDBclient::sendMultipleData(const std::vector<nlohmann::json> &entries, const std::string &collection) const{
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
  getCollection(collection).store_multi(dump, ids.data()); 
  if(m_stats) m_stats->add("provdb_client_sendmulti_send_ms", timer.elapsed_ms());

  return ids;
}

std::vector<uint64_t> ProvDBclient::sendMultipleData(const nlohmann::json &entries, const std::string &collection) const{
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

  getCollection(collection).store_multi(dump, ids.data()); 
  return ids;
}  
  

void ProvDBclient::sendDataAsync(const nlohmann::json &entry, const std::string &collection, OutstandingRequest *req) const{
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
  
  getCollection(collection).store(entry.dump(), ids, false, sreq);
}

void ProvDBclient::sendMultipleDataAsync(const std::vector<std::string> &entries, const std::string &collection, OutstandingRequest *req) const{
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

  getCollection(collection).store_multi(entries, ids, false, sreq); 

  if(m_stats) m_stats->add("provdb_client_sendmulti_async_send_ms", timer.elapsed_ms());
}



void ProvDBclient::sendMultipleDataAsync(const std::vector<nlohmann::json> &entries, const std::string &collection, OutstandingRequest *req) const{
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

  sendMultipleDataAsync(dump, collection, req);
}

void ProvDBclient::sendMultipleDataAsync(const nlohmann::json &entries, const std::string &collection, OutstandingRequest *req) const{
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

  sendMultipleDataAsync(dump, collection, req);
}  






bool ProvDBclient::retrieveData(nlohmann::json &entry, uint64_t index, const std::string &collection) const{
  if(m_is_connected){
    std::string data;
    try{
      getCollection(collection).fetch(index, &data);
    }catch(const sonata::Exception &e){
      return false;
    }
    entry = nlohmann::json::parse(data);
    return true;
  }
  return false;
}

void ProvDBclient::waitForSendComplete(){
  if(m_is_connected)
    anom_send_man.waitAll();
}

std::vector<std::string> ProvDBclient::retrieveAllData(const std::string &collection) const{
  std::vector<std::string> out;
  if(m_is_connected)
    getCollection(collection).all(&out);
  return out;
}

std::vector<std::string> ProvDBclient::filterData(const std::string &collection, const std::string &query) const{
  std::vector<std::string> out;
  if(m_is_connected)
    getCollection(collection).filter(query, &out);
  return out;
}

std::unordered_map<std::string,std::string> ProvDBclient::execute(const std::string &code, const std::unordered_set<std::string>& vars) const{
  std::unordered_map<std::string,std::string> out;
  if(m_is_connected)
    m_database.execute(code, vars, &out);
  return out;
}

void ProvDBclient::stopServer() const{
  verboseStream << "ProvDBclient" << m_instance_descr << " stopping server" << std::endl;
  m_stop_server->on(m_server)();
}

#ifdef ENABLE_MARGO_STATE_DUMP
void ProvDBclient::serverDumpState() const{
  verboseStream << "ProvDBclient" << m_instance_descr << " requesting server dump its state" << std::endl;
  m_margo_dump->on(m_server)();
}
#endif
    
#endif
  

