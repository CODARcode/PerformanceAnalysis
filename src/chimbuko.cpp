#include <limits>
#include "chimbuko/chimbuko.hpp"
#include "chimbuko/verbose.hpp"
#include "chimbuko/util/error.hpp"
#include "chimbuko/util/time.hpp"
#include "chimbuko/util/memutils.hpp"

using namespace chimbuko;

ChimbukoParams::ChimbukoParams(): rank(-1234),  //not set!
				  program_idx(0),
				  verbose(true),
				  outlier_sigma(6.),
				  trace_engineType("BPFile"), trace_data_dir("."), trace_inputFile("TAU_FILENAME-BINARYNAME"),
				  trace_connect_timeout(60),
				  pserver_addr(""), hpserver_nthr(1),
				  anom_win_size(10),
#ifdef ENABLE_PROVDB
				  provdb_addr(""), nprovdb_shards(1),
#endif
				  prov_outputpath(""),
				  perf_outputpath(""), perf_step(10),
				  only_one_frame(false), interval_msec(0),
				  err_outputpath(""), parser_beginstep_timeout(30), override_rank(false),
                                  outlier_statistic("exclusive_runtime"),
                                  step_report_freq(1)  
{}


void ChimbukoParams::print() const{
  std::cout << "Program Idx: " << program_idx
	    << "\nRank       : " << rank
	    << "\nEngine     : " << trace_engineType
	    << "\nBP in dir  : " << trace_data_dir
	    << "\nBP file    : " << trace_inputFile
#ifdef _USE_ZMQNET
	    << "\nPS Addr    : " << pserver_addr
#endif
	    << "\nSigma      : " << outlier_sigma
	    << "\nWindow size: " << anom_win_size
	  
	    << "\nInterval   : " << interval_msec << " msec"
	    << "\nPerf. metric outpath : " << perf_outputpath
	    << "\nPerf. step   : " << perf_step;
#ifdef ENABLE_PROVDB
  if(provdb_addr.size()){
    std::cout << "\nProvDB addr: " << provdb_addr
	      << "\nProvDB shards: " << nprovdb_shards;
  }
#endif
  if(prov_outputpath.size())
    std::cout << "\nProvenance outpath : " << prov_outputpath;
  
  std::cout << std::endl;
}


Chimbuko::Chimbuko(): m_parser(nullptr), m_event(nullptr), m_outlier(nullptr), m_io(nullptr), m_net_client(nullptr),
#ifdef ENABLE_PROVDB
		      m_provdb_client(nullptr),
#endif
		      m_normalevent_prov(nullptr),
		      m_metadata_parser(nullptr),
		      m_is_initialized(false){}

Chimbuko::~Chimbuko(){
  finalize();
}

void Chimbuko::initialize(const ChimbukoParams &params){
  PerfTimer total_timer, timer;

  if(m_is_initialized) finalize();
  m_params = params;

  //Check parameters
  if(m_params.rank < 0) throw std::runtime_error("Rank not set or invalid");

#ifdef ENABLE_PROVDB
  if(m_params.provdb_addr.size() == 0 && m_params.prov_outputpath.size() == 0) 
    throw std::runtime_error("Neither provenance database address or provenance output dir are set - no provenance data will be written!");
#else
  if(m_params.prov_outputpath.size() == 0) 
    throw std::runtime_error("Provenance output dir is not set - no provenance data will be written!");
#endif  
  
  //Initialize error collection
  if(params.err_outputpath.size())
    set_error_output_file(m_params.rank, stringize("%s/ad_error.%d", params.err_outputpath.c_str(), m_params.program_idx));
  else
    set_error_output_stream(m_params.rank, &std::cerr);

  //Connect to the provenance database and/or initialize provenance IO
#ifdef ENABLE_PROVDB
  timer.start();
  init_provdb();
  m_perf.add("ad_initialize_provdb_us", timer.elapsed_us());
#endif
  init_io(); //will write provenance info if provDB not in use
  init_normalevent_prov();
  
  //Connect to the parameter server
  timer.start();
  init_net_client();
  m_perf.add("ad_initialize_net_client_us", timer.elapsed_us());

  //Connect to TAU; process will be blocked at this line until it finds writer (in SST mode)
  timer.start();
  init_parser();
  m_perf.add("ad_initialize_parser_us", timer.elapsed_us());

  //Event and outlier objects must be initialized in order after parser
  init_event(); //requires parser
  init_outlier(); //requires event
  init_counter(); //requires parser

  init_metadata_parser();
  
  m_is_initialized = true;
  
  headProgressStream(m_params.rank) << "driver rank " << m_params.rank << " has initialized successfully" << std::endl;
  m_perf.add("ad_initialize_total_us", total_timer.elapsed_us());
}


void Chimbuko::init_io(){
  m_io = new ADio(m_params.program_idx, m_params.rank);
  m_io->setDispatcher();
  m_io->setDestructorThreadWaitTime(0); //don't know why we would need a wait

  if(m_params.prov_outputpath.size())
    m_io->setOutputPath(m_params.prov_outputpath); 
}

void Chimbuko::init_parser(){
  headProgressStream(m_params.rank) << "driver rank " << m_params.rank << " connecting to application trace stream" << std::endl;
  if(m_params.pserver_addr.length() > 0 && !m_net_client) throw std::runtime_error("Net client must be initialized before calling init_parser");
  m_parser = new ADParser(m_params.trace_data_dir + "/" + m_params.trace_inputFile, m_params.program_idx, m_params.rank, m_params.trace_engineType,
			  m_params.trace_connect_timeout);
  m_parser->linkPerf(&m_perf);  
  m_parser->setBeginStepTimeout(m_params.parser_beginstep_timeout);
  m_parser->setDataRankOverride(m_params.override_rank);
  m_parser->linkNetClient(m_net_client); //allow the parser to talk to the pserver to obtain global function indices
  headProgressStream(m_params.rank) << "driver rank " << m_params.rank << " successfully connected to application trace stream" << std::endl;
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
    headProgressStream(m_params.rank) << "driver rank " << m_params.rank << " connecting to parameter server" << std::endl;

    //If using the hierarchical PS we need to choose the appropriate port to connect to as an offset of the base port
    if(m_params.hpserver_nthr <= 0) throw std::runtime_error("Chimbuko::init_net_client Input parameter hpserver_nthr cannot be <1");
    else if(m_params.hpserver_nthr > 1){
      std::string orig = m_params.pserver_addr;
      m_params.pserver_addr = getHPserverIP(m_params.pserver_addr, m_params.hpserver_nthr, m_params.rank);
      verboseStream << "AD rank " << m_params.rank << " connecting to endpoint " << m_params.pserver_addr << " (base " << orig << ")" << std::endl;
    }

    m_net_client = new ADNetClient;
    m_net_client->linkPerf(&m_perf);
#ifdef _USE_MPINET
    m_net_client->connect_ps(m_params.rank);
#else
    m_net_client->connect_ps(m_params.rank, 0, m_params.pserver_addr);
#endif
    if(!m_net_client->use_ps()) fatal_error("Could not connect to parameter server");

    headProgressStream(m_params.rank) << "driver rank " << m_params.rank << " successfully connected to parameter server" << std::endl;
  }
}


void Chimbuko::init_outlier(){
  if(!m_event) throw std::runtime_error("Event managed must be initialized before calling init_outlier");

  ADOutlier::OutlierStatistic stat;
  if(m_params.outlier_statistic == "exclusive_runtime") stat = ADOutlier::ExclusiveRuntime;
  else if(m_params.outlier_statistic == "inclusive_runtime") stat = ADOutlier::InclusiveRuntime;
  else{ fatal_error("Invalid statistic"); }
  
  m_outlier = new ADOutlierSSTD(stat);
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
  if(m_params.provdb_addr.length() > 0){
    headProgressStream(m_params.rank) << "driver rank " << m_params.rank << " connecting to provenance database" << std::endl;
    m_provdb_client->connect(m_params.provdb_addr, m_params.nprovdb_shards);
    headProgressStream(m_params.rank) << "driver rank " << m_params.rank << " successfully connected to provenance database" << std::endl;
  }

  m_provdb_client->linkPerf(&m_perf);
}
#endif

void Chimbuko::init_normalevent_prov(){
  m_normalevent_prov = new ADNormalEventProvenance;
}

void Chimbuko::init_metadata_parser(){
  m_metadata_parser = new ADMetadataParser;
}

void Chimbuko::finalize()
{
  PerfTimer timer;
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

  if (m_normalevent_prov) delete m_normalevent_prov;

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

  m_perf.add("ad_finalize_total_us", timer.elapsed_us());
  m_perf.write();
}

//Returns false if beginStep was not successful
bool Chimbuko::parseInputStep(int &step, 
			      unsigned long long& n_func_events, 
			      unsigned long long& n_comm_events,
			      unsigned long long& n_counter_events
			      ){
  if (!m_parser->getStatus()) return false;

  PerfTimer timer, total_timer;
  total_timer.start();

  int expect_step = step+1;

  //Decide whether to report step progress
  bool do_step_report = enableVerboseLogging() || (m_params.step_report_freq > 0 && expect_step % m_params.step_report_freq == 0);
  
  if(do_step_report){ headProgressStream(m_params.rank) << "driver rank " << m_params.rank << " commencing step " << expect_step << std::endl; }

  timer.start();
  m_parser->beginStep();
  if (!m_parser->getStatus()){
    verboseStream << "driver parser appears to have disconnected, ending" << std::endl;
    return false;
  }
  m_perf.add("ad_parse_input_begin_step_us", timer.elapsed_us());
    
  // get trace data via SST
  step = m_parser->getCurrentStep();
  if(step != expect_step){ recoverable_error(stringize("Got step %d expected %d\n", step, expect_step)); }
    
  if(do_step_report){ headProgressStream(m_params.rank) << "driver rank " << m_params.rank << " commencing input parse for step " << step << std::endl; }

  verboseStream << "driver rank " << m_params.rank << " updating attributes" << std::endl;  
  timer.start();
  m_parser->update_attributes();
  m_perf.add("ad_parse_input_update_attributes_us", timer.elapsed_us());

  verboseStream << "driver rank " << m_params.rank << " fetching func data" << std::endl;  
  timer.start();
  m_parser->fetchFuncData();
  m_perf.add("ad_parse_input_fetch_func_data_us", timer.elapsed_us());

  verboseStream << "driver rank " << m_params.rank << " fetching comm data" << std::endl;  
  timer.start();
  m_parser->fetchCommData();
  m_perf.add("ad_parse_input_fetch_comm_data_us", timer.elapsed_us());

  verboseStream << "driver rank " << m_params.rank << " fetching counter data" << std::endl;  
  timer.start();
  m_parser->fetchCounterData();
  m_perf.add("ad_parse_input_fetch_counter_data_us", timer.elapsed_us());


  verboseStream << "driver rank " << m_params.rank << " finished gathering data" << std::endl;  


  // early SST buffer release
  m_parser->endStep();

  // count total number of events
  n_func_events += (unsigned long long)m_parser->getNumFuncData();
  n_comm_events += (unsigned long long)m_parser->getNumCommData();
  n_counter_events += (unsigned long long)m_parser->getNumCounterData();

  //Parse the new metadata for any attributes we want to maintain
  m_metadata_parser->addData(m_parser->getNewMetaData());

  verboseStream << "driver completed input parse for step " << step << std::endl;
  m_perf.add("ad_parse_input_total_us", total_timer.elapsed_us());

  return true;
}







//Extract parsed events and insert into the event manager
void Chimbuko::extractEvents(unsigned long &first_event_ts,
			     unsigned long &last_event_ts,
			     int step){
  PerfTimer timer;
  std::vector<Event_t> events = m_parser->getEvents();
  m_perf.add("ad_extract_events_get_events_us", timer.elapsed_us());
  timer.start();
  for(auto &e : events)
    m_event->addEvent(e);
  m_perf.add("ad_extract_events_register_us", timer.elapsed_us());
  m_perf.add("ad_extract_events_event_count", events.size());
  if(events.size()){
    first_event_ts = events.front().ts();
    last_event_ts = events.back().ts();
  }else{
    first_event_ts = last_event_ts = -1; //no events!
  }
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
  m_perf.add("ad_extract_counters_get_register_us", timer.elapsed_us());
} 


void Chimbuko::extractAndSendProvenance(const Anomalies &anomalies, 
					const int step,
					const unsigned long first_event_ts,
					const unsigned long last_event_ts) const{
  constexpr bool do_delete = true;
  constexpr bool add_outstanding = true;

  //Gather provenance data on anomalies and send to provenance database
  if(m_params.prov_outputpath.length() > 0
#ifdef ENABLE_PROVDB
     || m_provdb_client->isConnected()
#endif
     ){
    PerfTimer timer,timer2;

    //Put new normal event provenance into m_normalevent_prov
    timer.start();
    for(auto norm_it : anomalies.allEvents(Anomalies::EventType::Normal)){
      timer2.start();
      ADAnomalyProvenance extract_prov(*norm_it, *m_event, *m_outlier->get_global_parameters(), 
				       *m_counter, *m_metadata_parser, m_params.anom_win_size,
				       step, first_event_ts, last_event_ts);
      m_normalevent_prov->addNormalEvent(norm_it->get_pid(), norm_it->get_rid(), norm_it->get_tid(), norm_it->get_fid(), extract_prov.get_json());
      m_perf.add("ad_extract_send_prov_normalevent_update_per_event_us", timer2.elapsed_us());
    }
    m_perf.add("ad_extract_send_prov_normalevent_update_total_us", timer.elapsed_us());

    //Get any outstanding normal events from previous timesteps that we couldn't previously provide
    timer.start();
    std::vector<nlohmann::json> normalevent_prov = m_normalevent_prov->getOutstandingRequests(do_delete); //allow deletion of internal copy of events that are returned
    m_perf.add("ad_extract_send_prov_normalevent_get_outstanding_us", timer.elapsed_us());

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
      m_perf.add("ad_extract_send_prov_anom_data_generation_per_anom_us", timer2.elapsed_us());
      
      //Get the associated normal event
      //if normal event not available put into the list of outstanding requests
      //if normal event is available, delete internal copy within m_normalevent_prov so the normal event isn't added more than once
      timer2.start();
      auto nev = m_normalevent_prov->getNormalEvent(anom_it->get_pid(), anom_it->get_rid(), anom_it->get_tid(), anom_it->get_fid(), add_outstanding, do_delete); 
      if(nev.second) normalevent_prov.push_back(std::move(nev.first));
      m_perf.add("ad_extract_send_prov_normalevent_gather_per_anom_us", timer2.elapsed_us());
    }
    m_perf.add("ad_extract_send_prov_provenance_data_generation_total_us", timer.elapsed_us());

    if(anomaly_prov.size() > 0){
      timer.start();
      m_io->writeJSON(anomaly_prov, step, "anomalies");
      m_perf.add("ad_extract_send_prov_anom_data_io_write_us", timer.elapsed_us());

#ifdef ENABLE_PROVDB      
      timer.start();
      m_provdb_client->sendMultipleDataAsync(anomaly_prov, ProvenanceDataType::AnomalyData); //non-blocking send
      m_perf.add("ad_extract_send_prov_anom_data_send_async_us", timer.elapsed_us());
#endif
    }

    if(normalevent_prov.size() > 0){
      timer.start();
      m_io->writeJSON(normalevent_prov, step, "normalexecs");
      m_perf.add("ad_extract_send_prov_normalexec_data_io_write_us", timer.elapsed_us());

#ifdef ENABLE_PROVDB      
      timer.start();
      m_provdb_client->sendMultipleDataAsync(normalevent_prov, ProvenanceDataType::NormalExecData); //non-blocking send
      m_perf.add("ad_extract_send_prov_normalexec_data_send_async_us", timer.elapsed_us());
#endif
    }

  }//isConnected
}

void Chimbuko::sendNewMetadataToProvDB(int step) const{
  if(m_params.prov_outputpath.length() > 0
#ifdef ENABLE_PROVDB
     || m_provdb_client->isConnected()
#endif
     ){
    PerfTimer timer;
    std::vector<MetaData_t> const & new_metadata = m_parser->getNewMetaData();
    std::vector<nlohmann::json> new_metadata_j(new_metadata.size());
    for(size_t i=0;i<new_metadata.size();i++)
      new_metadata_j[i] = new_metadata[i].get_json();
    m_perf.add("ad_send_new_metadata_to_provdb_metadata_gather_us", timer.elapsed_us());

    if(new_metadata_j.size() > 0){
      timer.start();
      m_io->writeJSON(new_metadata_j, step, "metadata");
      m_perf.add("ad_send_new_metadata_to_provdb_io_write_us", timer.elapsed_us());
      
#ifdef ENABLE_PROVDB
      timer.start();
      m_provdb_client->sendMultipleDataAsync(new_metadata_j, ProvenanceDataType::Metadata); //non-blocking send
      m_perf.add("ad_send_new_metadata_to_provdb_send_async_us", timer.elapsed_us());
#endif
    }

  }
}

void Chimbuko::run(unsigned long long& n_func_events, 
		   unsigned long long& n_comm_events,
		   unsigned long long& n_counter_events,
		   unsigned long& n_outliers,
		   unsigned long& frames){
  if(!m_is_initialized) throw std::runtime_error("Chimbuko is not initialized");

  int step = m_parser->getCurrentStep(); //gives -1 as initial value. step+1 is the expected value of step in parseInputStep and is used as a (non-fatal) check
  unsigned long first_event_ts, last_event_ts; //earliest and latest timestamps in io frame

  std::string ad_perf = "ad_perf_" + std::to_string(m_params.rank) + ".json";
  m_perf.setWriteLocation(m_params.perf_outputpath, ad_perf);

  std::string ad_perf_prd = "ad_perf_prd_" + std::to_string(m_params.rank) + ".log";
  m_perf_prd.setWriteLocation(m_params.perf_outputpath, ad_perf_prd);
  
#if defined(_PERF_METRIC) && defined(ENABLE_PROVDB)
  
  std::ofstream *perf_provdb_client_outstanding = nullptr;
  if(m_params.rank == 0 && m_provdb_client->isConnected()) perf_provdb_client_outstanding = new std::ofstream(m_params.perf_outputpath + "/ad_provdb_outstandingreq.0.txt");
#endif
  
  PerfTimer step_timer, timer;

  //Loop until we lose connection with the application
  while ( parseInputStep(step, n_func_events, n_comm_events, n_counter_events) ) {
    //Decide whether to report step progress
    bool do_step_report = enableVerboseLogging() || (m_params.step_report_freq > 0 && step % m_params.step_report_freq == 0);

    if(do_step_report){ headProgressStream(m_params.rank) << "driver rank " << m_params.rank << " starting analysis of step " << step << std::endl; }
    step_timer.start();

    //Extract counters and put into counter manager
    extractCounters(m_params.rank, step);

    //Extract parsed events into event manager
    extractEvents(first_event_ts, last_event_ts, step);

    //Run the outlier detection algorithm on the events
    timer.start();
    Anomalies anomalies = m_outlier->run(step);
    m_perf.add("ad_run_anom_detection_time_us", timer.elapsed_us());

    n_outliers += anomalies.nEvents(Anomalies::EventType::Outlier);
    frames++;
    
    //Generate anomaly provenance for detected anomalies and send to DB
    extractAndSendProvenance(anomalies, step, first_event_ts, last_event_ts);
    
    //Send any new metadata to the DB
    sendNewMetadataToProvDB(step);
    
    if(m_net_client && m_net_client->use_ps()){
      //Gather function profile and anomaly statistics and send to the pserver
      ADLocalFuncStatistics prof_stats(m_params.program_idx, m_params.rank, step, &m_perf);
      prof_stats.gatherStatistics(m_event->getExecDataMap());
      prof_stats.gatherAnomalies(anomalies);
      prof_stats.updateGlobalStatistics(*m_net_client);

      //Gather counter statistics and send to pserver
      ADLocalCounterStatistics count_stats(m_params.program_idx, step, nullptr, &m_perf); //currently collect all counters
      count_stats.gatherStatistics(m_counter->getCountersByIndex());
      count_stats.updateGlobalStatistics(*m_net_client);
    }
    
    //Trim the call list
    timer.start();
    delete m_event->trimCallList(m_params.anom_win_size); //we keep the last $anom_win_size events for each thread so that we can extend the anomaly window capture into the previous io step
    m_perf.add("ad_run_trim_calllist_us", timer.elapsed_us());

    m_perf.add("ad_run_total_step_time_excl_parse_us", step_timer.elapsed_us());

    //Record periodic performance data
    if(m_params.perf_step > 0 && (step+1) % m_params.perf_step == 0){
      //Record the number of outstanding requests as a function of time, which can be used to gauge whether the throughput of the provDB is sufficient
#ifdef ENABLE_PROVDB
      m_perf_prd.add("provdb_incomplete_async_sends", m_provdb_client->getNoutstandingAsyncReqs());
#endif
      //Get the "data" memory usage (stack + heap)
      size_t total, resident;
      getMemUsage(total, resident);
      m_perf_prd.add("ad_mem_usage_kB", resident);

      //These only write if both filename and output path is set
      m_perf_prd.write();
      m_perf.write(); //periodically write out aggregated perf stats also
    }

    if (m_params.only_one_frame)
      break;

    if (m_params.interval_msec)
      std::this_thread::sleep_for(std::chrono::milliseconds(m_params.interval_msec));
    
    if(do_step_report){ headProgressStream(m_params.rank) << "driver rank " << m_params.rank << " completed analysis of step " << step << std::endl; }
  } // end of parser while loop

  //Always dump perf at end
  m_perf.write();
  
  headProgressStream(m_params.rank) << "driver rank " << m_params.rank << " run complete" << std::endl;
}
