#include <limits>
#include "chimbuko/chimbuko.hpp"
#include "chimbuko/verbose.hpp"

using namespace chimbuko;

void ChimbukoParams::print() const{
  std::cout << "\n" 
	    << "Program Idx: " << program_idx << "\n"
	    << "Rank       : " << rank << "\n"
	    << "Engine     : " << trace_engineType << "\n"
	    << "BP in dir  : " << trace_data_dir << "\n"
	    << "BP file    : " << trace_inputFile << "\n"
#ifdef _USE_ZMQNET
	    << "\nPS Addr    : " << pserver_addr
#endif
	    << "\nVIS Addr   : " << viz_addr
	    << "\nSigma      : " << outlier_sigma
	    << "\nWindow size: " << anom_win_size
	  
	    << "\nInterval   : " << interval_msec << " msec\n"
#ifdef ENABLE_PROVDB
	    << "\nProvDB addr: " << provdb_addr << "\n"
#endif
	    << "Perf. metric outpath : " << perf_outputpath << "\n"
	    << "Perf. step   : " << perf_step << "\n"
	    << std::endl;
}


Chimbuko::Chimbuko(): m_parser(nullptr), m_event(nullptr), m_outlier(nullptr), m_io(nullptr), m_net_client(nullptr),
#ifdef ENABLE_PROVDB
		      m_provdb_client(nullptr),
		      m_normalevent_prov(nullptr),
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
    m_io->setWinSize(m_params.anom_win_size);
    if ((m_params.viz_iomode == IOMode::Online || m_params.viz_iomode == IOMode::Both) && m_params.viz_addr.size()){
      m_io->open_curl(m_params.viz_addr);
    }

    if ((m_params.viz_iomode == IOMode::Offline || m_params.viz_iomode == IOMode::Both) && m_params.viz_datadump_outputPath.size()){
      m_io->setOutputPath(m_params.viz_datadump_outputPath);
    }
}

void Chimbuko::init_parser(){
  m_parser = new ADParser(m_params.trace_data_dir + "/" + m_params.trace_inputFile, m_params.program_idx, m_params.rank, m_params.trace_engineType);
  m_parser->linkPerf(&m_perf);  
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
    //If using the hierarchical PS we need to choose the appropriate port to connect to as an offset of the base port
    if(m_params.hpserver_nthr <= 0) throw std::runtime_error("Chimbuko::init_net_client Input parameter hpserver_nthr cannot be <1");
    else if(m_params.hpserver_nthr > 1){
      std::string orig = m_params.pserver_addr;
      m_params.pserver_addr = getHPserverIP(m_params.pserver_addr, m_params.hpserver_nthr, m_params.rank);
      VERBOSE(std::cout << "AD rank " << m_params.rank << " connecting to endpoint " << m_params.pserver_addr << " (base " << orig << ")" << std::endl);
    }

    m_net_client = new ADNetClient;
    m_net_client->linkPerf(&m_perf);
#ifdef _USE_MPINET
    m_net_client->connect_ps(m_params.rank);
#else
    m_net_client->connect_ps(m_params.rank, 0, m_params.pserver_addr);
#endif
    m_parser->linkNetClient(m_net_client); //allow the parser to talk to the pserver to obtain global function indices
  }
}


void Chimbuko::init_outlier(){
  if(!m_event) throw std::runtime_error("Event managed must be initialized before calling init_outlier");
  
  m_outlier = new ADOutlierSSTD();
  m_outlier->linkExecDataMap(m_event->getExecDataMap()); //link the map of function index to completed calls such that they can be tagged as outliers if appropriate
  m_outlier->set_sigma(m_params.outlier_sigma);
  if(m_net_client) m_outlier->linkNetworkClient(m_net_client);
  m_outlier->linkPerf(&m_perf);
}

void Chimbuko::init_counter(){
  if(!m_parser) throw std::runtime_error("Parser must be initialized before calling init_counter");  
  m_counter = new ADCounter();
  m_counter->linkCounterMap(m_parser->getCounterMap());
}

#ifdef ENABLE_PROVDB
void Chimbuko::init_provdb(){
  m_provdb_client = new ADProvenanceDBclient(m_params.rank);
  if(m_params.provdb_addr.length() > 0)
    m_provdb_client->connect(m_params.provdb_addr);
  m_normalevent_prov = new ADNormalEventProvenance;
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
  if (m_normalevent_prov) delete m_normalevent_prov;
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
  VERBOSE(std::cout << "driver commencing input parse for step " << step << std::endl);

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

  VERBOSE(std::cout << "driver completed input parse for step " << step << std::endl);

  return true;
}







//Extract parsed events and insert into the event manager
void Chimbuko::extractEvents(unsigned long &first_event_ts,
			     unsigned long &last_event_ts,
			     int step){
  PerfTimer timer;
  std::vector<Event_t> events = m_parser->getEvents();
  m_perf.add("parser_event_extract_us", timer.elapsed_us());
  timer.start();
  for(auto &e : events)
    m_event->addEvent(e);
  m_perf.add("parser_event_register_us", timer.elapsed_us());
  m_perf.add("parser_event_count", events.size());
  first_event_ts = events.front().ts();
  last_event_ts = events.back().ts();
}
  
void Chimbuko::extractCounters(int rank, int step){
  if(!m_counter) throw std::runtime_error("Counter is not initialized");
  PerfTimer timer;
  for(size_t c=0;c<m_parser->getNumCounterData();c++){
    Event_t ev(m_parser->getCounterData(c),
	       EventDataType::COUNT,
	       c,
	       generate_event_id(rank, step, c));    
    m_counter->addCounter(ev);
  }
  m_perf.add("parser_counter_extract_register_us", timer.elapsed_us());
} 


#ifdef ENABLE_PROVDB

void Chimbuko::extractAndSendProvenance(const Anomalies &anomalies, 
					const int step,
					const unsigned long first_event_ts,
					const unsigned long last_event_ts) const{
  constexpr bool do_delete = true;
  constexpr bool add_outstanding = true;

  //Gather provenance data on anomalies and send to provenance database
  if(m_provdb_client->isConnected()){
    PerfTimer timer,timer2;

    //Put new normal event provenance into m_normalevent_prov
    timer.start();
    for(auto norm_it : anomalies.allEvents(Anomalies::EventType::Normal)){
      timer2.start();
      ADAnomalyProvenance extract_prov(*norm_it, *m_event, *m_outlier->get_global_parameters(), 
				       *m_counter, *m_metadata_parser, m_params.anom_win_size,
				       step, first_event_ts, last_event_ts);
      m_normalevent_prov->addNormalEvent(norm_it->get_pid(), norm_it->get_rid(), norm_it->get_tid(), norm_it->get_fid(), extract_prov.get_json());
      m_perf.add("provdb_normalevent_update_per_event_us", timer2.elapsed_us());
    }
    m_perf.add("provdb_normalevent_update_total_us", timer.elapsed_us());

    //Get any outstanding normal events from previous timesteps that we couldn't previously provide
    timer.start();
    std::vector<nlohmann::json> normalevent_prov = m_normalevent_prov->getOutstandingRequests(do_delete); //allow deletion of internal copy of events that are returned
    m_perf.add("provdb_normalevent_get_outstanding_us", timer.elapsed_us());

    //Gather provenance of anomalies and for each one try to obtain a normal execution
    timer.start();
    std::vector<nlohmann::json> anomaly_prov(anomalies.nEvents(Anomalies::EventType::Outlier));
    size_t i=0;
    for(auto anom_it : anomalies.allEvents(Anomalies::EventType::Outlier)){
      timer2.start();
      ADAnomalyProvenance extract_prov(*anom_it, *m_event, *m_outlier->get_global_parameters(), 
				       *m_counter, *m_metadata_parser, m_params.anom_win_size,
				       step, first_event_ts, last_event_ts);
      anomaly_prov[i++] = extract_prov.get_json();
      m_perf.add("provdb_anom_data_generation_per_anom_us", timer2.elapsed_us());
      
      //Get the associated normal event
      //if normal event not available put into the list of outstanding requests
      //if normal event is available, delete internal copy within m_normalevent_prov so the normal event isn't added more than once
      timer2.start();
      auto nev = m_normalevent_prov->getNormalEvent(anom_it->get_pid(), anom_it->get_rid(), anom_it->get_tid(), anom_it->get_fid(), add_outstanding, do_delete); 
      if(nev.second) normalevent_prov.push_back(std::move(nev.first));
      m_perf.add("provdb_anom_normalevent_gather_per_anom_us", timer2.elapsed_us());
    }
    m_perf.add("provdb_provenance_data_generation_total_us", timer.elapsed_us());
      
    timer.start();
    m_provdb_client->sendMultipleDataAsync(anomaly_prov, ProvenanceDataType::AnomalyData); //non-blocking send
    m_perf.add("provdb_anom_data_send_async_us", timer.elapsed_us());

    timer.start();
    m_provdb_client->sendMultipleDataAsync(normalevent_prov, ProvenanceDataType::NormalExecData); //non-blocking send
    m_perf.add("provdb_normalexec_data_send_async_us", timer.elapsed_us());
  }//isConnected
}

void Chimbuko::sendNewMetadataToProvDB() const{
  if(m_provdb_client->isConnected()){
    PerfTimer timer;
    std::vector<MetaData_t> const & new_metadata = m_parser->getNewMetaData();
    std::vector<nlohmann::json> new_metadata_j(new_metadata.size());
    for(size_t i=0;i<new_metadata.size();i++)
      new_metadata_j[i] = new_metadata[i].get_json();
    m_perf.add("provdb_metadata_gather_us", timer.elapsed_us());
    timer.start();
    m_provdb_client->sendMultipleDataAsync(new_metadata_j, ProvenanceDataType::Metadata); //non-blocking send
    m_perf.add("provdb_metadata_send_async_us", timer.elapsed_us());
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
  unsigned long first_event_ts, last_event_ts; //earliest and latest timestamps in io frame

  std::string ad_perf = "ad_perf_" + std::to_string(m_params.rank) + ".json";
  m_perf.setWriteLocation(m_params.perf_outputpath, ad_perf);
  
  PerfTimer step_timer;

  //Loop until we lose connection with the application
  while ( parseInputStep(step, n_func_events, n_comm_events, n_counter_events) ) {
    step_timer.start();

    //Extract counters and put into counter manager
    extractCounters(m_params.rank, step);

    //Extract parsed events into event manager
    extractEvents(first_event_ts, last_event_ts, step);

    //Run the outlier detection algorithm on the events
    PerfTimer anom_timer;
    Anomalies anomalies = m_outlier->run(step);
    m_perf.add("anom_detection_time_us", anom_timer.elapsed_us());

    n_outliers += anomalies.nEvents(Anomalies::EventType::Outlier);
    frames++;
    

#ifdef ENABLE_PROVDB
    //Generate anomaly provenance for detected anomalies and send to DB
    extractAndSendProvenance(anomalies, step, first_event_ts, last_event_ts);
    
    //Send any new metadata to the DB
    sendNewMetadataToProvDB();
#endif	
    
    if(m_net_client && m_net_client->use_ps()){
      //Gather function profile and anomaly statistics and send to the pserver
      ADLocalFuncStatistics prof_stats(m_params.program_idx, m_params.rank, step, &m_perf);
      prof_stats.gatherStatistics(m_event->getExecDataMap());
      prof_stats.gatherAnomalies(anomalies);
      prof_stats.updateGlobalStatistics(*m_net_client);

      //Gather counter statistics and send to pserver
      ADLocalCounterStatistics count_stats(step, nullptr, &m_perf); //currently collect all counters
      count_stats.gatherStatistics(m_counter->getCountersByIndex());
      count_stats.updateGlobalStatistics(*m_net_client);
    }
    


    //Dump data accumulated during IO step
    m_io->write(m_event->trimCallList(), step);
    m_io->writeCounters(m_counter->flushCounters(), step);
    m_io->writeMetaData(m_parser->getNewMetaData(), step);

    m_perf.add("total_step_time_us", step_timer.elapsed_us());

    if(m_params.perf_step > 0 && (step+1) % m_params.perf_step == 0)
      m_perf.write(); //only writes if filename/output path set

    if (m_params.only_one_frame)
      break;

    if (m_params.interval_msec)
      std::this_thread::sleep_for(std::chrono::microseconds(m_params.interval_msec));
  } // end of parser while loop

  //Always dump perf at end
  m_perf.write();

}
