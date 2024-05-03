#include <limits>
#include "chimbuko/core/chimbuko.hpp"
#include "chimbuko/core/verbose.hpp"
#include "chimbuko/core/util/error.hpp"
#include "chimbuko/core/util/time.hpp"
#include "chimbuko/core/util/memutils.hpp"

using namespace chimbuko;

ChimbukoBaseParams::ChimbukoBaseParams(): rank(-1234),  //not set!
				  ana_obj_idx(0),
				  verbose(true),
				  outlier_sigma(6.),
                                  net_recv_timeout(30000),
				  pserver_addr(""), hpserver_nthr(1),
				  ad_algorithm("hbos"),
				  hbos_threshold(0.99),
				  hbos_use_global_threshold(true),
				  hbos_max_bins(200),
#ifdef ENABLE_PROVDB
				  provdb_addr_dir(""), nprovdb_shards(1), nprovdb_instances(1), provdb_mercury_auth_key(""),
#endif
				  prov_outputpath(""),
                                  prov_record_startstep(-1),
                                  prov_record_stopstep(-1),  
				  perf_outputpath(""), perf_step(10),
				  only_one_frame(false), interval_msec(0),
				  err_outputpath(""), 
                                  step_report_freq(1),
                                  analysis_step_freq(1),
                                  max_frames(-1),
                                  prov_io_freq(1),
                                  global_model_sync_freq(1),
                                  ps_send_stats_freq(1)
{}


void ChimbukoBaseParams::print() const{
  std::cout << "AD Algorithm: " << ad_algorithm
	    << "\nAnalysis Objective Idx: " << ana_obj_idx
	    << "\nRank       : " << rank
#ifdef _USE_ZMQNET
	    << "\nPS Addr    : " << pserver_addr
#endif
	    << "\nSigma      : " << outlier_sigma
	    << "\nHBOS/COPOD Threshold: " << hbos_threshold
	    << "\nUsing Global threshold: " << hbos_use_global_threshold
	    << "\nInterval   : " << interval_msec << " msec"
            << "\nNetClient Receive Timeout : " << net_recv_timeout << "msec"
	    << "\nPerf. metric outpath : " << perf_outputpath
	    << "\nPerf. step   : " << perf_step
            << "\nAnalysis step freq. : " << analysis_step_freq ;
#ifdef ENABLE_PROVDB
  if(provdb_addr_dir.size()){
    std::cout << "\nProvDB addr dir: " << provdb_addr_dir
	      << "\nProvDB shards: " << nprovdb_shards
      	      << "\nProvDB server instances: " << nprovdb_instances;
  }
#endif
  if(prov_outputpath.size())
    std::cout << "\nProvenance outpath : " << prov_outputpath;

  std::cout << std::endl;
}


ChimbukoParams::ChimbukoParams(): trace_engineType("BPFile"), trace_data_dir("."), trace_inputFile("TAU_FILENAME-BINARYNAME"),
				  trace_connect_timeout(60), override_rank(false),
				  anom_win_size(10),
                                  outlier_statistic("exclusive_runtime"),
                                  read_ignored_corrid_funcs(""),
                                  func_threshold_file(""),
                                  ignored_func_file(""),
                                  monitoring_watchlist_file(""),
                                  monitoring_counter_prefix(""),
				  parser_beginstep_timeout(30),
                                  prov_min_anom_time(0)
{}

void ChimbukoParams::print() const{
  base_params.print();
  std::cout << "\nEngine     : " << trace_engineType
	    << "\nBP in dir  : " << trace_data_dir
	    << "\nBP file    : " << trace_inputFile
	    << "\nWindow size: " << anom_win_size
	    << std::endl;
}


ChimbukoBase::ChimbukoBase(): 
  m_outlier(nullptr), m_io(nullptr), m_net_client(nullptr),
#ifdef ENABLE_PROVDB
  m_provdb_client(nullptr),
#endif
  m_base_is_initialized(false)
{}
ChimbukoBase::~ChimbukoBase(){
  finalizeBase();
}


#ifdef ENABLE_PROVDB
void ChimbukoBase::init_provdb(){
  if(m_base_params.provdb_mercury_auth_key != ""){
    headProgressStream(m_base_params.rank) << "driver rank " << m_base_params.rank << " setting Mercury authorization key to \"" << m_base_params.provdb_mercury_auth_key << "\"" << std::endl;
    ADProvenanceDBengine::setMercuryAuthorizationKey(m_base_params.provdb_mercury_auth_key);
  }
  
  m_provdb_client = new ADProvenanceDBclient(m_base_params.rank);
  if(m_base_params.provdb_addr_dir.length() > 0){
    headProgressStream(m_base_params.rank) << "driver rank " << m_base_params.rank << " connecting to provenance database" << std::endl;
    m_provdb_client->connectMultiServer(m_base_params.provdb_addr_dir, m_base_params.nprovdb_shards, m_base_params.nprovdb_instances);
    headProgressStream(m_base_params.rank) << "driver rank " << m_base_params.rank << " successfully connected to provenance database" << std::endl;
  }

  m_provdb_client->linkPerf(&m_perf);
  m_ptr_registry.registerPointer(m_provdb_client);
}
#endif

void ChimbukoBase::init_io(){
  m_io = new ADio(m_base_params.ana_obj_idx, m_base_params.rank);
  m_io->setDispatcher();
  m_io->setDestructorThreadWaitTime(0); //don't know why we would need a wait

  if(m_base_params.prov_outputpath.size())
    m_io->setOutputPath(m_base_params.prov_outputpath);
  m_ptr_registry.registerPointer(m_io);
}

void ChimbukoBase::init_net_client(){
  if(m_base_params.pserver_addr.length() > 0){
    headProgressStream(m_base_params.rank) << "driver rank " << m_base_params.rank << " connecting to parameter server" << std::endl;

    //If using the hierarchical PS we need to choose the appropriate port to connect to as an offset of the base port
    if(m_base_params.hpserver_nthr <= 0) throw std::runtime_error("Chimbuko::init_net_client Input parameter hpserver_nthr cannot be <1");
    else if(m_base_params.hpserver_nthr > 1){
      std::string orig = m_base_params.pserver_addr;
      m_base_params.pserver_addr = getHPserverIP(m_base_params.pserver_addr, m_base_params.hpserver_nthr, m_base_params.rank);
      verboseStream << "AD rank " << m_base_params.rank << " connecting to endpoint " << m_base_params.pserver_addr << " (base " << orig << ")" << std::endl;
    }

    m_net_client = new ADThreadNetClient;
    m_net_client->linkPerf(&m_perf);
    m_net_client->setRecvTimeout(m_base_params.net_recv_timeout);
    m_net_client->connect_ps(m_base_params.rank, 0, m_base_params.pserver_addr);
    if(!m_net_client->use_ps()) fatal_error("Could not connect to parameter server");
    m_ptr_registry.registerPointer(m_net_client);

    headProgressStream(m_base_params.rank) << "driver rank " << m_base_params.rank << " successfully connected to parameter server" << std::endl;
  }
}


void ChimbukoBase::init_outlier(){
  ADOutlier::AlgoParams params;
  params.hbos_thres = m_base_params.hbos_threshold;
  params.glob_thres = m_base_params.hbos_use_global_threshold;
  params.sstd_sigma = m_base_params.outlier_sigma;
  params.hbos_max_bins = m_base_params.hbos_max_bins;
  //params.func_threshold_file = m_base_params.func_threshold_file;

  m_outlier = ADOutlier::set_algorithm(m_base_params.rank, m_base_params.ad_algorithm, params);
  if(m_net_client) m_outlier->linkNetworkClient(m_net_client);
  m_outlier->linkPerf(&m_perf);
  m_outlier->setGlobalModelSyncFrequency(m_base_params.global_model_sync_freq);
  m_ptr_registry.registerPointer(m_outlier);

  //Read ignored functions
  // if(m_base_params.ignored_func_file.size()){
  //   std::ifstream in(m_base_params.ignored_func_file);
  //   if(in.is_open()) {
  //     std::string func;
  //     while (std::getline(in, func)){
  // 	headProgressStream(m_base_params.rank) << "Skipping anomaly detection for function \"" << func << "\"" << std::endl;
  // 	m_outlier->setIgnoreFunction(func);
  //     }
  //     in.close();
  //   }else{
  //     fatal_error("Failed to open ignored-func file " + m_base_params.ignored_func_file);
  //   }
  // }


}



void ChimbukoBase::initializeBase(const ChimbukoBaseParams &params){
  if(m_base_is_initialized) fatal_error("Cannot reinitialized base without finalizing"); //expect the derived class to call finalizeBase
  m_base_params = params;

  //Check parameters
  if(m_base_params.rank < 0) throw std::runtime_error("Rank not set or invalid");
  
#ifdef ENABLE_PROVDB
  if(m_base_params.provdb_addr_dir.size() == 0 && m_base_params.prov_outputpath.size() == 0)
    fatal_error("Neither provenance database address or provenance output dir are set - no provenance data will be written!");
#else
  if(m_base_params.prov_outputpath.size() == 0)
    fatal_error("Provenance output dir is not set - no provenance data will be written!");
#endif

  //Setup perf output
  std::string ad_perf = stringize("ad_perf.%d.%d.json", m_base_params.ana_obj_idx, m_base_params.rank);
  m_perf.setWriteLocation(m_base_params.perf_outputpath, ad_perf);

  std::string ad_perf_prd = stringize("ad_perf_prd.%d.%d.log", m_base_params.ana_obj_idx, m_base_params.rank);
  m_perf_prd.setWriteLocation(m_base_params.perf_outputpath, ad_perf_prd);

  //Initialize error collection
  if(m_base_params.err_outputpath.size())
    set_error_output_file(m_base_params.rank, stringize("%s/ad_error.%d", m_base_params.err_outputpath.c_str(), m_base_params.ana_obj_idx));
  else
    set_error_output_stream(m_base_params.rank, &std::cerr);

  PerfTimer timer;

  //Connect to the provenance database and/or initialize provenance IO
#ifdef ENABLE_PROVDB
  timer.start();
  init_provdb();
  m_perf.add("ad_initialize_provdb_ms", timer.elapsed_ms());
#endif
  init_io(); //will write provenance info if provDB not in use

  //Connect to the parameter server
  timer.start();
  init_net_client();
  m_perf.add("ad_initialize_net_client_ms", timer.elapsed_ms());

  init_outlier();

  m_step = 0;
  
  m_base_is_initialized = true;  
}

void ChimbukoBase::finalizeBase()
{
  if(!m_base_is_initialized) return;

  //Always dump perf at end
  m_perf.write();

  if(m_net_client) m_net_client->disconnect_ps();

  m_ptr_registry.resetPointers(); //delete all pointers in reverse order to which they were instantiated
  m_ptr_registry.deregisterPointers(); //allow them to be re-registered if init is called again

  m_base_is_initialized = false;
}

ADNetClient & ChimbukoBase::getNetClient() const{
  if(m_net_client == nullptr) fatal_error("Parameter server is not in use, net client has not been created");
  return *m_net_client; 
}

Chimbuko::Chimbuko(): m_parser(nullptr), m_event(nullptr), 
		      m_anomaly_provenance(nullptr),
		      m_metadata_parser(nullptr),
		      m_monitoring(nullptr),
		      m_is_initialized(false){}

Chimbuko::~Chimbuko(){
  finalize();
}

void Chimbuko::initialize(const ChimbukoParams &params){
  PerfTimer total_timer, timer;

  if(m_is_initialized) finalize();

  //Always initialize base first
  this->initializeBase(params.base_params);

  m_params = params;

  m_program_idx = this->getAnalysisObjectiveIdx();

  //Reset state
  m_execdata_first_event_ts = m_execdata_last_event_ts = 0;
  m_execdata_first_event_ts_set = false;
  m_accum_prd.reset();

  //Connect to TAU; process will be blocked at this line until it finds writer (in SST mode)
  timer.start();
  init_parser();
  this->getPerfRecorder().add("ad_initialize_parser_ms", timer.elapsed_ms());

  //Event and outlier objects must be initialized in order after parser
  init_metadata_parser();
  init_event(); //requires parser and metadata parser
  init_counter(); //requires parser
  init_monitoring(); //requires parser
  init_provenance_gatherer(); //requires most components

  m_is_initialized = true;

  headProgressStream(this->getRank()) << "driver rank " << this->getRank() << " has initialized successfully" << std::endl;
  this->getPerfRecorder().add("ad_initialize_total_ms", total_timer.elapsed_ms());
}

void Chimbuko::init_parser(){
  int rank = this->getRank();
  headProgressStream(rank) << "driver rank " << rank << " connecting to application trace stream" << std::endl;
  m_parser = new ADParser(m_params.trace_data_dir + "/" + m_params.trace_inputFile, m_program_idx, rank, m_params.trace_engineType,
			  m_params.trace_connect_timeout);
  m_parser->linkPerf(&this->getPerfRecorder());
  m_parser->setBeginStepTimeout(m_params.parser_beginstep_timeout);
  m_parser->setDataRankOverride(m_params.override_rank); //override the rank index in the input data
  if(this->usePS()) m_parser->linkNetClient(&this->getNetClient()); //allow the parser to talk to the pserver to obtain global function indices
  m_ptr_registry.registerPointer(m_parser);
  headProgressStream(rank) << "driver rank " << rank << " successfully connected to application trace stream" << std::endl;
}

void Chimbuko::init_event(){
  if(!m_parser) fatal_error("Parser must be initialized before calling init_event");
  if(!m_metadata_parser) fatal_error("Metadata parser must be initialized before calling init_event");
  m_event = new ADEvent(this->getBaseParams().verbose);
  m_event->linkFuncMap(m_parser->getFuncMap());
  m_event->linkEventType(m_parser->getEventType());
  m_event->linkCounterMap(m_parser->getCounterMap());
  m_event->linkGPUthreadMap(&m_metadata_parser->getGPUthreadMap());
  m_ptr_registry.registerPointer(m_event);

  //Setup ignored correlation IDs for given functions
  if(m_params.read_ignored_corrid_funcs.size()){
    std::ifstream in(m_params.read_ignored_corrid_funcs);
    if(in.is_open()) {
      std::string func;
      while (std::getline(in, func)){
	headProgressStream(this->getRank()) << "Ignoring correlation IDs for function \"" << func << "\"" << std::endl;
	m_event->ignoreCorrelationIDsForFunction(func);
      }
      in.close();
    }else{
      fatal_error("Failed to open ignored-corried-func file " + m_params.read_ignored_corrid_funcs);
    }

  }
}


void Chimbuko::init_counter(){
  if(!m_parser) throw std::runtime_error("Parser must be initialized before calling init_counter");
  m_counter = new ADCounter();
  m_counter->linkCounterMap(m_parser->getCounterMap());
  m_ptr_registry.registerPointer(m_counter);
}

void Chimbuko::init_metadata_parser(){
  m_metadata_parser = new ADMetadataParser;
  m_ptr_registry.registerPointer(m_metadata_parser);
}

void Chimbuko::init_monitoring(){
  m_monitoring = new ADMonitoring;
  m_monitoring->linkCounterMap(m_parser->getCounterMap());
  if(m_params.monitoring_watchlist_file.size())
    m_monitoring->parseWatchListFile(m_params.monitoring_watchlist_file);
  else
    m_monitoring->setDefaultWatchList();
  if(m_params.monitoring_counter_prefix.size())
    m_monitoring->setCounterPrefix(m_params.monitoring_counter_prefix);
  m_ptr_registry.registerPointer(m_monitoring);
}

void Chimbuko::init_provenance_gatherer(){
  m_anomaly_provenance = new ADAnomalyProvenance(*m_event);
  m_anomaly_provenance->linkPerf(&this->getPerfRecorder());
  m_anomaly_provenance->linkAlgorithmParams(this->getAD().get_global_parameters());
  m_anomaly_provenance->linkMonitoring(m_monitoring);
  m_anomaly_provenance->linkMetadata(m_metadata_parser);
  m_anomaly_provenance->setWindowSize(m_params.anom_win_size);
  m_anomaly_provenance->setMinimumAnomalyTime(m_params.prov_min_anom_time);
  m_ptr_registry.registerPointer(m_anomaly_provenance);
}


void Chimbuko::finalize()
{
  PerfTimer timer;
  if(!m_is_initialized) return;

  //For debug purposes, write out any unmatched correlation ID events that we encountered as recoverable errors
  if(m_event->getUnmatchCorrelationIDevents().size() > 0){
    for(auto c: m_event->getUnmatchCorrelationIDevents()){
      std::stringstream ss;
      ss << "Unmatched correlation ID: " << c.first << " from event " << c.second->get_json().dump();
      recoverable_error(ss.str());
    }
  }

  headProgressStream(this->getRank()) << "driver rank " << this->getRank() << " run complete" << std::endl;

  m_ptr_registry.resetPointers(); //delete all pointers in reverse order to which they were instantiated
  m_ptr_registry.deregisterPointers(); //allow them to be re-registered if init is called again

  m_exec_ignore_counters.clear(); //reset the ignored counters list
  
  m_is_initialized = false;

  //Always finalize base last
  this->finalizeBase();

  this->getPerfRecorder().add("ad_finalize_total_ms", timer.elapsed_ms());
  this->getPerfRecorder().write();
}

//Returns false if beginStep was not successful
bool Chimbuko::parseInputStep(unsigned long long& n_func_events,
			      unsigned long long& n_comm_events,
			      unsigned long long& n_counter_events,
			      const int expect_step
			      ){
  if (!m_parser->getStatus()) return false;

  int rank = this->getRank();
  auto &perf = this->getPerfRecorder();

  PerfTimer timer, total_timer;
  total_timer.start();

  //Decide whether to report step progress
  bool do_step_report = this->doStepReport(expect_step);

  if(do_step_report){ headProgressStream(rank) << "driver rank " << rank << " commencing step " << expect_step << std::endl; }

  timer.start();
  m_parser->beginStep();
  if (!m_parser->getStatus()){
    verboseStream << "driver parser appears to have disconnected, ending" << std::endl;
    return false;
  }
  perf.add("ad_parse_input_begin_step_ms", timer.elapsed_ms());

  // get trace data via SST
  int step = m_parser->getCurrentStep();
  if(step != expect_step){ recoverable_error(stringize("Got step %d expected %d\n", step, expect_step)); }

  if(do_step_report){ headProgressStream(rank) << "driver rank " << rank << " commencing input parse for step " << step << std::endl; }

  verboseStream << "driver rank " << rank << " updating attributes" << std::endl;
  timer.start();
  m_parser->update_attributes();
  perf.add("ad_parse_input_update_attributes_ms", timer.elapsed_ms());

  verboseStream << "driver rank " << rank << " fetching func data" << std::endl;
  timer.start();
  m_parser->fetchFuncData();
  perf.add("ad_parse_input_fetch_func_data_ms", timer.elapsed_ms());

  verboseStream << "driver rank " << rank << " fetching comm data" << std::endl;
  timer.start();
  m_parser->fetchCommData();
  perf.add("ad_parse_input_fetch_comm_data_ms", timer.elapsed_ms());

  verboseStream << "driver rank " << rank << " fetching counter data" << std::endl;
  timer.start();
  m_parser->fetchCounterData();
  perf.add("ad_parse_input_fetch_counter_data_ms", timer.elapsed_ms());


  verboseStream << "driver rank " << rank << " finished gathering data" << std::endl;


  // early SST buffer release
  m_parser->endStep();

  // count total number of events
  n_func_events = (unsigned long long)m_parser->getNumFuncData();
  n_comm_events = (unsigned long long)m_parser->getNumCommData();
  n_counter_events = (unsigned long long)m_parser->getNumCounterData();

  //Parse the new metadata for any attributes we want to maintain
  m_metadata_parser->addData(m_parser->getNewMetaData());

  verboseStream << "driver completed input parse for step " << step << std::endl;
  perf.add("ad_parse_input_total_ms", total_timer.elapsed_ms());

  return true;
}







//Extract parsed events and insert into the event manager
void Chimbuko::extractEvents(unsigned long &first_event_ts,
			     unsigned long &last_event_ts,
			     int step){
  auto &perf = this->getPerfRecorder();
  PerfTimer timer;
  std::vector<Event_t> events = m_parser->getEvents();
  perf.add("ad_extract_events_get_events_ms", timer.elapsed_ms());
  timer.start();
  for(auto &e : events){
    if(e.type() != EventDataType::COUNT || !m_exec_ignore_counters.count(e.counter_id()) ){
      m_event->addEvent(e);
    }
  }
  perf.add("ad_extract_events_register_ms", timer.elapsed_ms());
  perf.add("ad_extract_events_event_count", events.size());
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
	       eventID(rank, step, c));
    try{
      m_counter->addCounter(ev);
    }catch(const std::exception &e){
      recoverable_error(std::string("extractCounters failed to register counter event :\"") + ev.get_json().dump() + "\"");
    }
  }
  this->getPerfRecorder().add("ad_extract_counters_get_register_ms", timer.elapsed_ms());
}


void Chimbuko::extractNodeState(){
  if(!m_monitoring) throw std::runtime_error("Monitoring is not initialized");
  if(!m_counter) throw std::runtime_error("Counter is not initialized");
  PerfTimer timer;
  m_monitoring->extractCounters(m_counter->getCountersByIndex());

  //Get the counters that monitoring is watching and tell the event manager to ignore them so they don't get attached to any function execution
  std::vector<int> cidx_mon_ignore = m_monitoring->getMonitoringCounterIndices();
  for(int i: cidx_mon_ignore){
    m_exec_ignore_counters.insert(i);
  }
  
  this->getPerfRecorder().add("ad_extract_node_state_ms", timer.elapsed_ms());
}




void ChimbukoBase::bufferStoreProvenanceData(const ADDataInterface &anomalies){
  //Optionally skip provenance data recording on certain steps
  if(m_base_params.prov_record_startstep != -1 && m_step < m_base_params.prov_record_startstep) return;
  if(m_base_params.prov_record_stopstep != -1 && m_step > m_base_params.prov_record_stopstep) return;
 
  //Gather provenance data on anomalies and send to provenance database
  if(m_base_params.prov_outputpath.length() > 0
#ifdef ENABLE_PROVDB
     || m_provdb_client->isConnected()
#endif
     ){
    this->bufferStoreProvenanceDataImplementation(m_provdata_buf, anomalies, m_step);
  }//isConnected
}


void Chimbuko::bufferStoreProvenanceDataImplementation(std::unordered_map<std::string, std::vector<nlohmann::json> > &buffer, const ADDataInterface &anomalies, const int step){
  //Get the provenance data
  PerfTimer timer, tot_timer;

  //Anomalies/Normal execs
  std::vector<nlohmann::json> anomaly_prov, normalevent_prov;
  m_anomaly_provenance->getProvenanceEntries(anomaly_prov, normalevent_prov, static_cast<const ADExecDataInterface &>(anomalies), step, m_execdata_first_event_ts, m_execdata_last_event_ts);
  
  buffer["anomalies"].insert(buffer["anomalies"].end(), anomaly_prov.begin(), anomaly_prov.end());
  buffer["normalexecs"].insert(buffer["normalexecs"].end(), normalevent_prov.begin(), normalevent_prov.end());
  this->getPerfRecorder().add("ad_extract_buffer_anom_data_ms", timer.elapsed_ms());

  //Metadata
  timer.start();
  std::vector<MetaData_t> const & new_metadata = m_parser->getNewMetaData();
  if(new_metadata.size() > 0){
    std::vector<nlohmann::json> new_metadata_j(new_metadata.size());
    for(size_t i=0;i<new_metadata.size();i++)
      new_metadata_j[i] = new_metadata[i].get_json();
    buffer["metadata"].insert(buffer["metadata"].end(), new_metadata_j.begin(), new_metadata_j.end());
  }
  this->getPerfRecorder().add("ad_extract_buffer_meta_data_ms", timer.elapsed_ms());

  this->getPerfRecorder().add("ad_extract_buffer_provenance_data_total_ms", tot_timer.elapsed_ms());
}




void ChimbukoBase::sendProvenance(bool force){
  if(
     (m_base_params.prov_outputpath.length() > 0
#ifdef ENABLE_PROVDB
      || m_provdb_client->isConnected()
#endif
      )
     && 
     ( (m_step + m_base_params.rank) % m_base_params.prov_io_freq == 0 || force ) //stagger sends over ranks by offsetting by rank index
     ){
    int rank = m_base_params.rank;

    //Get the provenance data
    verboseStream << "Chimbuko rank " << rank << " performing send of provenance data on step " << m_step << std::endl;
    
    PerfTimer timer;
    //Write and send provenance data
    if(m_provdata_buf["anomalies"].size() > 0){
      timer.start();
      m_io->writeJSON(m_provdata_buf["anomalies"], m_step, "anomalies");
      m_perf.add("ad_extract_send_prov_anom_data_io_write_ms", timer.elapsed_ms());

#ifdef ENABLE_PROVDB
      timer.start();
      m_provdb_client->sendMultipleDataAsync(m_provdata_buf["anomalies"], ProvenanceDataType::AnomalyData); //non-blocking send
      m_perf.add("ad_extract_send_prov_anom_data_send_async_ms", timer.elapsed_ms());
#endif
    }

    if(m_provdata_buf["normalexecs"].size() > 0){
      timer.start();
      m_io->writeJSON(m_provdata_buf["normalexecs"], m_step, "normalexecs");
      m_perf.add("ad_extract_send_prov_normalexec_data_io_write_ms", timer.elapsed_ms());
	
#ifdef ENABLE_PROVDB
      timer.start();
      m_provdb_client->sendMultipleDataAsync(m_provdata_buf["normalexecs"], ProvenanceDataType::NormalExecData); //non-blocking send
      m_perf.add("ad_extract_send_prov_normalexec_data_send_async_ms", timer.elapsed_ms());
#endif
    }

    if(m_provdata_buf["metadata"].size() > 0){
      timer.start();
      m_io->writeJSON(m_provdata_buf["metadata"], m_step, "metadata");
      m_perf.add("ad_send_new_metadata_to_provdb_io_write_ms", timer.elapsed_ms());

#ifdef ENABLE_PROVDB
      timer.start();
      m_provdb_client->sendMultipleDataAsync(m_provdata_buf["metadata"], ProvenanceDataType::Metadata); //non-blocking send
      m_perf.add("ad_send_new_metadata_to_provdb_metadata_count", m_provdata_buf["metadata"].size());
      m_perf.add("ad_send_new_metadata_to_provdb_send_async_ms", timer.elapsed_ms());
#endif
    }
    
    m_provdata_buf.clear();
  }//isConnected
}




void Chimbuko::bufferStorePSdata(const ADDataInterface &anomalies,  const int step){
  if(this->usePS()){
    PerfTimer timer;
    int rank = this->getRank();
    auto &perf = this->getPerfRecorder();

    const ADExecDataInterface &ei = dynamic_cast<const ADExecDataInterface &>(anomalies);

    //Gather function profile and anomaly statistics
    timer.start();
    ADLocalFuncStatistics func_stats(m_program_idx, rank, step, &perf);
    func_stats.gatherStatistics(m_event->getExecDataMap());
    func_stats.gatherAnomalies(ei);
    m_funcstats_buf.emplace_back(std::move(func_stats));
    perf.add("ad_gather_ps_data_gather_profile_stats_time_ms", timer.elapsed_ms());

    //Gather counter statistics
    timer.start();
    ADLocalCounterStatistics count_stats(m_program_idx, step, nullptr, &perf); //currently collect all counters
    count_stats.gatherStatistics(m_counter->getCountersByIndex());
    m_countstats_buf.emplace_back(std::move(count_stats));
    perf.add("ad_gather_data_gather_counter_stats_time_ms", timer.elapsed_ms());

    //Gather anomaly metrics
    timer.start();
    ADLocalAnomalyMetrics metrics(m_program_idx, rank, step, m_execdata_first_event_ts, m_execdata_last_event_ts, ei);
    m_anom_metrics_buf.emplace_back(std::move(metrics));
    perf.add("ad_gather_ps_data_gather_metrics_time_ms", timer.elapsed_ms());
    perf.add("ad_gather_ps_data_total_time_ms", timer.elapsed_ms());
  }
}

void ChimbukoBase::sendPSdata(bool force){
  if(m_net_client && m_net_client->use_ps() && ( (m_step + this->getRank()) % m_base_params.ps_send_stats_freq == 0 || force) ){ //stagger sends by offsetting by rank
    this->sendPSdataImplementation(*m_net_client, m_step);
  }
}


void Chimbuko::sendPSdataImplementation(ADNetClient &net_client, const int step){
  if(m_funcstats_buf.size()){
    //Send the data in a single communication
    auto &perf = this->getPerfRecorder();
    int rank = this->getRank();
     
    verboseStream << "Chimbuko rank " << rank << " performing send of PS stats data on step " << step << std::endl;
    PerfTimer timer;
    timer.start();
    ADcombinedPSdataArray comb_stats(m_funcstats_buf, m_countstats_buf, m_anom_metrics_buf, &perf); 
    comb_stats.send(net_client); //serializes and puts result in buffer awaiting send so we are free to flush the buffers
    m_funcstats_buf.clear(); m_countstats_buf.clear(); m_anom_metrics_buf.clear();
    perf.add("ad_send_ps_data_total_time_ms", timer.elapsed_ms());
  }
}


bool ChimbukoBase::doStepReport(int step) const{
  return enableVerboseLogging() || (m_base_params.step_report_freq > 0 && step % m_base_params.step_report_freq == 0);
}
bool ChimbukoBase::doAnalysisOnStep(int step) const{
  return step % m_base_params.analysis_step_freq == 0;
}

bool Chimbuko::readStep(std::unique_ptr<ADDataInterface> &iface){
  if(!m_is_initialized) throw std::runtime_error("Chimbuko is not initialized");
  int rank = this->getRank();
  auto &perf = this->getPerfRecorder();

  PerfTimer timer;
  unsigned long io_step_first_event_ts, io_step_last_event_ts; //earliest and latest timestamps in io frame
  unsigned long long n_func_events_step, n_comm_events_step, n_counter_events_step; //event count in present step
  
  int step = this->getStep();

  if(!parseInputStep(n_func_events_step, n_comm_events_step, n_counter_events_step, step)) return false;

  //Increment total events
  m_run_stats.n_func_events += n_func_events_step;
  m_run_stats.n_comm_events += n_comm_events_step;
  m_run_stats.n_counter_events += n_counter_events_step;
  
  m_accum_prd.n_func_events += n_func_events_step;
  m_accum_prd.n_comm_events += n_comm_events_step;
  m_accum_prd.n_counter_events += n_counter_events_step;

  //Decide whether to report step progress
  bool do_step_report = this->doStepReport(step);
  
  if(do_step_report){ headProgressStream(rank) << "driver rank " << rank << " starting step " << step
							<< ". Event count: func=" << n_func_events_step << " comm=" << n_comm_events_step
							<< " counter=" << n_counter_events_step << std::endl; }
  //Extract counters and put into counter manager
  timer.start();
  extractCounters(rank, step);
  perf.add("ad_run_extract_counters_time_ms", timer.elapsed_ms());

  //Extract the node state
  extractNodeState();
  
  //Extract parsed events into event manager
  timer.start();
  extractEvents(io_step_first_event_ts, io_step_last_event_ts, step);
  perf.add("ad_run_extract_events_time_ms", timer.elapsed_ms());
    
  //Update the timestamp bounds for the analysis
  m_execdata_last_event_ts = io_step_last_event_ts;
  if(!m_execdata_first_event_ts_set){
    m_execdata_first_event_ts = io_step_first_event_ts;
    m_execdata_first_event_ts_set = true;
  }

  if(this->doAnalysisOnStep(step)){ //only need to generate the interface if we are going to run an analysis
    ADExecDataInterface::OutlierStatistic stat;
    if(m_params.outlier_statistic == "exclusive_runtime") stat = ADExecDataInterface::ExclusiveRuntime;
    else if(m_params.outlier_statistic == "inclusive_runtime") stat = ADExecDataInterface::InclusiveRuntime;
    else{ fatal_error("Invalid statistic"); }
    
    ADExecDataInterface *data_iface = new ADExecDataInterface(m_event->getExecDataMap(), stat);
    if( (this->getBaseParams().ad_algorithm == "sst" || this->getBaseParams().ad_algorithm == "SST") && std::getenv("CHIMBUKO_DISABLE_CUDA_JIT_WORKAROUND") == nullptr )
      data_iface->setIgnoreFirstFunctionCall(&m_func_seen);
    iface.reset(data_iface);
  }
  return true;
}

bool ChimbukoBase::runFrame(){
  PerfTimer step_timer, timer;

  std::unique_ptr<ADDataInterface> iface;
  if(!this->readStep(iface)) return false;

  m_accum_prd.n_steps++; //count steps since last dump
  
  bool do_step_report = doStepReport(m_step);

  //Are we running the analysis this step?
  bool do_run_analysis = doAnalysisOnStep(m_step);

  if(do_run_analysis){
    //Run the outlier detection algorithm on the events
    timer.start();
    m_outlier->run(*iface,m_step);
    m_perf.add("ad_run_anom_detection_time_ms", timer.elapsed_ms());
    m_perf.add("ad_run_anomaly_count", iface->nEventsRecorded(ADDataInterface::EventType::Outlier));
    m_perf.add("ad_run_n_exec_analyzed", iface->nEvents());

#ifdef _PERF_METRIC
    {
      auto pu = m_perf.getMetrics().getLastRecorded("param_update_ms");
      if(!pu.first){ recoverable_error("Could not obtain information on parameter update/sync time"); }
      else m_accum_prd.pserver_sync_time_ms += pu.second;
    }
#endif

    int nout = iface->nEventsRecorded(ADDataInterface::EventType::Outlier);
    int nnormal = iface->nEvents() - nout; //this is the total number of normal events, not just of those that were recorded
    m_run_stats.n_outliers += nout;
    m_accum_prd.n_outliers += nout;

    //Generate anomaly provenance for detected anomalies and send to DB
    timer.start();
    bufferStoreProvenanceData(*iface);
    m_perf.add("ad_run_extract_send_provenance_time_ms", timer.elapsed_ms());
      
    //Gather and send statistics and data to the pserver
    timer.start();
    this->bufferStorePSdata(*iface, m_step);
    m_perf.add("ad_run_gather_send_ps_data_time_ms", timer.elapsed_ms());

    this->performEndStepAction();

    if(do_step_report){ headProgressStream(m_base_params.rank) << "driver rank " << m_base_params.rank << " function execution analysis complete: total=" << nout + nnormal << " normal=" << nnormal << " anomalous=" << nout << std::endl; }
  }//if(do_run_analysis)

  //Send provenance and PS data if the step aligns with the send frequency. Want to do this even if not analyzing this step
  sendProvenance(); 
  sendPSdata();

  m_perf.add("ad_run_total_step_time_excl_parse_ms", step_timer.elapsed_ms());

  //Record periodic performance data
  if(m_base_params.perf_step > 0 && m_step % m_base_params.perf_step == 0){
    //Record the number of outstanding requests as a function of time, which can be used to gauge whether the throughput of the provDB is sufficient
#ifdef ENABLE_PROVDB
    m_perf_prd.add("provdb_incomplete_async_sends", m_provdb_client->getNoutstandingAsyncReqs());
#endif
    //Get the "data" memory usage (stack + heap)
    size_t total, resident;
    getMemUsage(total, resident);
    m_perf_prd.add("ad_mem_usage_kB", resident);

    m_perf_prd.add("io_steps", m_accum_prd.n_steps);

    //Write accumulated outlier count
    m_perf_prd.add("outlier_count", m_accum_prd.n_outliers);

    m_perf_prd.add("ps_sync_time_ms", m_accum_prd.pserver_sync_time_ms);

    //Reset the counts
    m_accum_prd.reset();

    //These only write if both filename and output path is set
    m_perf_prd.write();
    m_perf.write(); //periodically write out aggregated perf stats also
  }

  if(do_step_report){ headProgressStream(m_base_params.rank) << "driver rank " << m_base_params.rank << " completed step " << m_step << std::endl; }

  ++m_step;
  return true;
}


 void Chimbuko::performEndStepAction(){ 
    //Trim the call list
   PerfTimer timer;
   auto &perf = this->getPerfRecorder();
   m_event->purgeCallList(m_params.anom_win_size, &this->m_purge_report); //we keep the last $anom_win_size events for each thread so that we can extend the anomaly window capture into the previous io step
   perf.add("ad_run_purge_calllist_ms", timer.elapsed_ms());

   //Flush counters
   timer.start();
   delete m_counter->flushCounters();
   perf.add("ad_run_flush_counters_ms", timer.elapsed_ms());

   //Enable update of analysis time window bound on next step (analysis may be performed on multiple step's worth of data)
   m_execdata_first_event_ts_set = false;
 }

 void Chimbuko::recordResetPeriodicPerfData(PerfPeriodic &perf_prd){
    //Write out how many events remain in the ExecData and how many unmatched correlation IDs there are
   auto const &purge_report = this->m_purge_report;
   perf_prd.add("call_list_purged", purge_report.n_purged);
   perf_prd.add("call_list_kept_protected", purge_report.n_kept_protected);
   perf_prd.add("call_list_kept_incomplete", purge_report.n_kept_incomplete);
   perf_prd.add("call_list_kept_window", purge_report.n_kept_window);

   //Write accumulated event counts
   perf_prd.add("event_count_func", m_accum_prd.n_func_events);
   perf_prd.add("event_count_comm", m_accum_prd.n_comm_events);
   perf_prd.add("event_count_counter", m_accum_prd.n_counter_events);
    
   //m_perf_prd.add("call_list_carryover_size", m_event->getCallListSize());
   perf_prd.add("n_unmatched_correlation_id", m_event->getUnmatchCorrelationIDevents().size());
 }

int ChimbukoBase::run(){
  if( m_base_params.max_frames == 0 ) return 0;

  //Reset run statistics for base and derived
  m_run_stats.reset();
  this->resetRunStatistics();

  int frames = 0;
  
  while(runFrame()){
    ++frames; //number of times runFrame has been called *in this call to run*. This function can be called more than once, e.g. if we drop out early for one of the reasons below

    if (m_base_params.only_one_frame)
      break;

    if( m_base_params.max_frames > 0 && frames == m_base_params.max_frames )
      break;
    
    if (m_base_params.interval_msec)
      std::this_thread::sleep_for(std::chrono::milliseconds(m_base_params.interval_msec));
  }
  //send any outstanding buffered provenance and PS data (second arg forces send)
  sendProvenance(true);
  sendPSdata(true); 
  return frames;
}
