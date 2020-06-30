#include <limits>
#include "chimbuko/chimbuko.hpp"
#include "chimbuko/verbose.hpp"

using namespace chimbuko;

void ChimbukoParams::print() const{
  std::cout << "\n" 
	    << "rank       : " << rank << "\n"
	    << "Engine     : " << trace_engineType << "\n"
	    << "BP in dir  : " << trace_data_dir << "\n"
	    << "BP file    : " << trace_inputFile << "\n"
#ifdef _USE_ZMQNET
	    << "\nPS Addr    : " << pserver_addr
#endif
	    << "\nVIS Addr   : " << viz_addr
	    << "\nsigma      : " << outlier_sigma
	    << "\nwindow size: " << viz_anom_winSize
	  
	    << "\nInterval   : " << interval_msec << " msec\n"
#ifdef ENABLE_PROVDB
	    << "\nProvDB addr: " << provdb_addr << "\n"
#endif
#ifdef _PERF_METRIC
	    << "perf. metric outpath : " << perf_outputpath << "\n"
	    << "perf. step   : " << perf_step << "\n"
#endif
	    << std::endl;
}


Chimbuko::Chimbuko(): m_parser(nullptr), m_event(nullptr), m_outlier(nullptr), m_io(nullptr), m_net_client(nullptr),
#ifdef ENABLE_PROVDB
		      m_provdb_client(nullptr),
#endif
		      m_metadata_parser(nullptr),
		      m_is_initialized(false){}

Chimbuko::~Chimbuko(){
  finalize();
}

void Chimbuko::initialize(const ChimbukoParams &params){
  if(m_is_initialized) finalize();
  m_params = params;
  if(m_params.rank < 0) throw std::runtime_error("Rank not set or invalid");

  // First, init io to make sure file (or connection) handler
  init_io();

  // Second, init parser because it will hold shared memory with event and outlier object
  // also, process will be blocked at this line until it finds writer (in SST mode)
  init_parser();

  // Thrid, init event and outlier objects	
  init_event();
  init_net_client();
  init_outlier();
  init_counter();

#ifdef ENABLE_PROVDB
  init_provdb();
#endif
  
  init_metadata_parser();
  
  m_is_initialized = true;
}


void Chimbuko::init_io(){
    m_io = new ADio();
    m_io->setRank(m_params.rank);
    m_io->setDispatcher();
    m_io->setWinSize(m_params.viz_anom_winSize);
    if ((m_params.viz_iomode == IOMode::Online || m_params.viz_iomode == IOMode::Both) && m_params.viz_addr.size()){
      m_io->open_curl(m_params.viz_addr);
    }

    if ((m_params.viz_iomode == IOMode::Offline || m_params.viz_iomode == IOMode::Both) && m_params.viz_datadump_outputPath.size()){
      m_io->setOutputPath(m_params.viz_datadump_outputPath);
    }
}

void Chimbuko::init_parser(){
    m_parser = new ADParser(m_params.trace_data_dir + "/" + m_params.trace_inputFile, m_params.trace_engineType);
}

void Chimbuko::init_event(){
  if(!m_parser) throw std::runtime_error("Parser must be initialized before calling init_event");
  m_event = new ADEvent(m_params.verbose);
  m_event->linkFuncMap(m_parser->getFuncMap());
  m_event->linkEventType(m_parser->getEventType());
  m_event->linkCounterMap(m_parser->getCounterMap());
}

void Chimbuko::init_net_client(){
  if(m_params.pserver_addr.length() > 0){
    m_net_client = new ADNetClient;
#ifdef _USE_MPINET
    m_net_client->connect_ps(m_params.rank);
#else
    m_net_client->connect_ps(m_params.rank, 0, m_params.pserver_addr);
#endif
  }
}


void Chimbuko::init_outlier(){
  if(!m_event) throw std::runtime_error("Event managed must be initialized before calling init_outlier");
  
  m_outlier = new ADOutlierSSTD();
  m_outlier->linkExecDataMap(m_event->getExecDataMap()); //link the map of function index to completed calls such that they can be tagged as outliers if appropriate
  m_outlier->set_sigma(m_params.outlier_sigma);
  if(m_net_client) m_outlier->linkNetworkClient(m_net_client);
}

void Chimbuko::init_counter(){
  if(!m_parser) throw std::runtime_error("Parser must be initialized before calling init_counter");  
  m_counter = new ADCounter();
  m_counter->linkCounterMap(m_parser->getCounterMap());
}

#ifdef ENABLE_PROVDB
void Chimbuko::init_provdb(){
  m_provdb_client = new ADProvenanceDBclient;
  if(m_params.provdb_addr.length() > 0)
    m_provdb_client->connect(m_params.provdb_addr);
}
#endif

void Chimbuko::init_metadata_parser(){
  m_metadata_parser = new ADMetadataParser;
}

void Chimbuko::finalize()
{
  if(!m_is_initialized) return;

  if(m_net_client){
    m_net_client->disconnect_ps();
    delete m_net_client;
  }
  if (m_parser) delete m_parser;
  if (m_event) delete m_event;
  if (m_outlier) delete m_outlier;
  if (m_io) delete m_io;
  if (m_counter) delete m_counter;

#ifdef ENABLE_PROVDB
  if (m_provdb_client) delete m_provdb_client;
#endif

  if(m_metadata_parser) delete m_metadata_parser;

  m_parser = nullptr;
  m_event = nullptr;
  m_outlier = nullptr;
  m_io = nullptr;
  m_net_client = nullptr;
  m_counter = nullptr;
#ifdef ENABLE_PROVDB
  m_provdb_client = nullptr;
#endif
  m_metadata_parser = nullptr;

  m_is_initialized = false;
}

//Returns false if beginStep was not successful
bool Chimbuko::parseInputStep(int &step, 
			      unsigned long long& n_func_events, 
			      unsigned long long& n_comm_events,
			      unsigned long long& n_counter_events
			      ){
  if (!m_parser->getStatus()) return false;
  m_parser->beginStep();
  if (!m_parser->getStatus()) return false;
    
  // get trace data via SST
  step = m_parser->getCurrentStep();
  m_parser->update_attributes();
  m_parser->fetchFuncData();
  m_parser->fetchCommData();
  m_parser->fetchCounterData();

  // early SST buffer release
  m_parser->endStep();

  // count total number of events
  n_func_events += (unsigned long long)m_parser->getNumFuncData();
  n_comm_events += (unsigned long long)m_parser->getNumCommData();
  n_counter_events += (unsigned long long)m_parser->getNumCounterData();

  //Parse the new metadata for any attributes we want to maintain
  m_metadata_parser->addData(m_parser->getNewMetaData());

  return true;
}

/**
 * @brief Return the pointer to the array whose timestamp (given by the value in the array at the provided offset) is earliest
 * @param arrays A vector of array pointers
 * @param ts_offsets The elements of the arrays that correspond to the timestamp
 *
 * Some (but not all) arrays can be nullptr
 * If there is a tie between two entries, the array that enters first (lowest index) in the input vectors is chosen
 */
const unsigned long* getEarliest(const std::vector<const unsigned long*> &arrays, const std::vector<int> &ts_offsets){
  if(ts_offsets.size() != arrays.size()) throw std::runtime_error("Input parameter vectors have different sizes!");
  unsigned long earliest_ts = std::numeric_limits<unsigned long>::max();
  int earliest = -1;
  for(int i=0;i<arrays.size();i++){
    if(arrays[i] != nullptr &&
       arrays[i][ts_offsets[i]] < earliest_ts){
      earliest = i;
      earliest_ts = arrays[i][ts_offsets[i]];
    }
  }
  if(earliest == -1) throw std::runtime_error("Failed to determine earliest entry");
  return arrays[earliest];  
}


std::pair<Event_t,bool> createAndValidateEvent(const unsigned long * data, EventDataType t, size_t idx, std::string id,
					       ADParser * parser, int rank){
  // Create event
  Event_t ev(data, t, idx, id);

  // Validate the event
  // NOTE: in SST mode with large rank number (>=10), sometimes I got
  // very large number of pid, rid and tid. This issue was also observed in python version.
  // In BP mode, it doesn't have such problem. As temporal solution, we skip those data and
  // it doesn't cause any problems (e.g. call stack violation). Need to consult with 
  // adios team later
  bool good = true;

  if (!ev.valid() || ev.pid() > 1000000 || (int)ev.rid() != rank || ev.tid() >= 1000000){
    std::cout << "\n***** Invalid event *****\n";
    //std::cout << "[" << rank << "] " << ev << std::endl;
    if(ev.valid()){
      std::string event_type("UNKNOWN"), func_name("UNKNOWN");
      
      auto eit = parser->getEventType()->find(ev.eid());
      if(eit != parser->getEventType()->end())
	event_type = eit->second;
      
      auto fit = parser->getFuncMap()->find(ev.fid());
      if(fit != parser->getFuncMap()->end())
	func_name = fit->second;
      
      std::cerr << ev.get_json().dump() << std::endl << event_type << ": " << func_name << std::endl;
    }else std::cerr << "Data pointer is null" << std::endl;
    
    good = false;
  }
  return {ev, good};
}





//Extract parsed events and insert into the event manager
void Chimbuko::extractEvents(int rank, int step){
  //During the timestep a number of function and perhaps also comm and counter events occurred
  //The parser stores these events separately in order of their timestamp
  //We want to iterate through these events in order of their timestamp in order to correlate them
  size_t idx_funcData=0, idx_commData = 0, idx_counterData = 0;
  const unsigned long *funcData = nullptr;
  const unsigned long *commData = nullptr;
  const unsigned long *counterData = nullptr;

  funcData = m_parser->getFuncData(idx_funcData);
  commData = m_parser->getCommData(idx_commData);
  counterData = m_parser->getCounterData(idx_counterData);

  while (funcData != nullptr || commData != nullptr || counterData != nullptr){
    //This functionality should really be part of the event manager
    // Determine event to handle
    // - if both, funcData and commData, are available, select one that occurs earlier.
    // - priority is on funcData because communication might happen within a function.
    // - in the future, we may not need to process on commData (i.e. exclude it from execution data).
    const unsigned long *data = getEarliest( {funcData, commData, counterData}, {FUNC_IDX_TS, COMM_IDX_TS, COUNTER_IDX_TS} );

    if(data == funcData){
      std::pair<Event_t,bool> evp = createAndValidateEvent(data, EventDataType::FUNC, idx_funcData, generate_event_id(rank, step, idx_funcData), m_parser, rank);
      if(evp.second) m_event->addFunc(evp.first);
      funcData = m_parser->getFuncData(++idx_funcData);
    }else if(data == commData){
      std::pair<Event_t,bool> evp = createAndValidateEvent(data, EventDataType::COMM, idx_commData, "event_id", m_parser, rank);
      if(evp.second) m_event->addComm(evp.first);
      commData = m_parser->getCommData(++idx_commData);
    }else if(data == counterData){
      std::pair<Event_t,bool> evp = createAndValidateEvent(data, EventDataType::COUNT, idx_counterData, "event_id", m_parser, rank);
      if(evp.second) m_event->addCounter(evp.first);
      counterData = m_parser->getCounterData(++idx_counterData);
    }else{
      throw std::runtime_error("Unexpected pointer");
    }
  }//while loop over func and comm data
}
  
void Chimbuko::extractCounters(int rank, int step){
  if(!m_counter) throw std::runtime_error("Counter is not initialized");
  for(size_t c=0;c<m_parser->getNumCounterData();c++){
    Event_t ev(m_parser->getCounterData(c),
	       EventDataType::COUNT,
	       c,
	       generate_event_id(rank, step, c));    
    m_counter->addCounter(ev);
  }
} 


#ifdef ENABLE_PROVDB

void Chimbuko::extractAndSendProvenance(const Anomalies &anomalies) const{
  //Gather provenance data on anomalies and send to provenance database
  if(m_provdb_client->isConnected()){
    std::vector<nlohmann::json> anomaly_prov(anomalies.nOutliers());
    size_t i=0;
    for(auto anom_it : anomalies.allOutliers()){
      ADAnomalyProvenance extract_prov(*anom_it, *m_event, *m_outlier->get_global_parameters(), *m_counter, *m_metadata_parser);
      anomaly_prov[i++] = extract_prov.get_json();
    }
    m_provdb_client->sendMultipleDataAsync(anomaly_prov, ProvenanceDataType::AnomalyData); //non-blocking send
  }
}

void Chimbuko::sendNewMetadataToProvDB() const{
  if(m_provdb_client->isConnected()){
    std::vector<MetaData_t> const & new_metadata = m_parser->getNewMetaData();
    std::vector<nlohmann::json> new_metadata_j(new_metadata.size());
    for(size_t i=0;i<new_metadata.size();i++)
      new_metadata_j[i] = new_metadata[i].get_json();
    m_provdb_client->sendMultipleDataAsync(new_metadata_j, ProvenanceDataType::Metadata); //non-blocking send
  }
}



#endif





void Chimbuko::run(unsigned long long& n_func_events, 
		   unsigned long long& n_comm_events,
		   unsigned long long& n_counter_events,
		   unsigned long& n_outliers,
		   unsigned long& frames){
  if(!m_is_initialized) throw std::runtime_error("Chimbuko is not initialized");

  int step = 0; 

#ifdef _PERF_METRIC
  std::string ad_perf = "ad_perf_" + std::to_string(m_params.rank) + ".json";
  RunMetric perf;
  m_outlier->linkPerf(&perf);
#endif

  //Loop until we lose connection with the application
  while ( parseInputStep(step, n_func_events, n_comm_events, n_counter_events) ) {
    //Extract counters and put into counter manager
    extractCounters(m_params.rank, step);

    //Extract parsed events into event manager
    extractEvents(m_params.rank, step);

    //Run the outlier detection algorithm on the events
    Anomalies anomalies = m_outlier->run(step);
    n_outliers += anomalies.nOutliers();
    frames++;

#ifdef ENABLE_PROVDB
    //Generate anomaly provenance for detected anomalies and send to DB
    extractAndSendProvenance(anomalies);
    
    //Send any new metadata to the DB
    sendNewMetadataToProvDB();
#endif	

    //Gather function profile and anomaly statistics and send to the pserver
    if(m_net_client && m_net_client->use_ps()){
      ADLocalFuncStatistics prof_stats(step
#ifdef _PERF_METRIC
				       , &perf
#endif
				       );
      prof_stats.gatherStatistics(m_event->getExecDataMap());
      prof_stats.gatherAnomalies(anomalies);
      prof_stats.updateGlobalStatistics(*m_net_client);
    }
      
    //Dump data accumulated during IO step
    m_io->write(m_event->trimCallList(), step);
    m_io->writeCounters(m_counter->flushCounters(), step);
    m_io->writeMetaData(m_parser->getNewMetaData(), step);

#ifdef _PERF_METRIC
    // dump performance metric event perf_step steps
    if ( m_params.perf_outputpath.length() && m_params.perf_step > 0 && (step+1) % m_params.perf_step == 0 ) {
      perf.dump(m_params.perf_outputpath, ad_perf);
    }
#endif
    if (m_params.only_one_frame)
      break;

    if (m_params.interval_msec)
      std::this_thread::sleep_for(std::chrono::microseconds(m_params.interval_msec));
  } // end of parser while loop
}
