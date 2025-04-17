#include "chimbuko/modules/performance_analysis/chimbuko.hpp"
#include "chimbuko/modules/performance_analysis/provdb/ProvDBmoduleSetup.hpp"
#include "chimbuko/core/util/commandLineParser.hpp"
#include "chimbuko/core/ad/ADcmdLineArgs.hpp"
#ifdef USE_MPI
#include <mpi.h>
#endif

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

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
#ifdef ENABLE_PROVDB
  ProvDBmoduleSetup pdb_setup;
  this->initializeBase(params.base_params, pdb_setup);
#else
  this->initializeBase(params.base_params);
#endif

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


static void setupInterface(std::unique_ptr<ADDataInterface> &iface, const std::string &outlier_statistic, const std::string &algorithm,
			   ADExecDataInterface::FunctionsSeenType &func_seen, ExecDataMap_t const* exec_data_map){
  ADExecDataInterface::OutlierStatistic stat;
  if(outlier_statistic == "exclusive_runtime") stat = ADExecDataInterface::ExclusiveRuntime;
  else if(outlier_statistic == "inclusive_runtime") stat = ADExecDataInterface::InclusiveRuntime;
  else{ fatal_error("Invalid statistic"); }
    
  ADExecDataInterface *data_iface = new ADExecDataInterface(exec_data_map, stat);
  if( algorithm == "sstd" && std::getenv("CHIMBUKO_DISABLE_CUDA_JIT_WORKAROUND") == nullptr )
    data_iface->setIgnoreFirstFunctionCall(&func_seen);
  iface.reset(data_iface);
}


bool Chimbuko::readStep(std::unique_ptr<ADDataInterface> &iface){
  if(!m_is_initialized) throw std::runtime_error("Chimbuko is not initialized");
  int rank = this->getRank();
  auto &perf = this->getPerfRecorder();

  PerfTimer timer;
  unsigned long io_step_first_event_ts, io_step_last_event_ts; //earliest and latest timestamps in io frame
  unsigned long long n_func_events_step, n_comm_events_step, n_counter_events_step; //event count in present step
  
  int step = this->getStep();

  //Parse data
  if( parseInputStep(n_func_events_step, n_comm_events_step, n_counter_events_step, step) ){
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
  }

  //Check if we have any data to analyze (some might be remaining from previous steps)
  bool have_data = false;
  for(auto &fid_p : *m_event->getExecDataMap())
    if(fid_p.second.size() > 0){
      have_data = true;
      break;
    }
  verboseStream << "driver rank " << rank << " step " << step << " have_data=" << have_data << std::endl;
  
  if(!have_data)
    return false;
  
  if(this->doAnalysisOnStep(step)) //only need an interface if we will be running the analysis on this step
    setupInterface(iface, m_params.outlier_statistic, this->getAD().getAlgorithmName(), m_func_seen, m_event->getExecDataMap());
    
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

   if(enableVerboseLogging()){
     size_t cexec = 0;
     for(auto &fid_p : *m_event->getExecDataMap())
       cexec += fid_p.second.size();

     size_t nunlabeled =0;
     size_t nlocked = 0;
     size_t nincomplete = 0;
     size_t nunknown = 0;
     const CallListMap_p_t &call_list = m_event->getCallListMap();
     const ExecDataMap_t* exec_data = m_event->getExecDataMap();

     bool stream_has_ended = !m_parser->getStatus();
     
     for (auto const& it_p : call_list) {
       for (auto const& it_r : it_p.second) {
	 for (auto const& it_t: it_r.second) {
	   CallList_t const& cl = it_t.second;
	   for(auto const &call : cl){
	     if(call.get_exit() == -1) ++nincomplete;
	     else if(call.get_label() == 0){
	       ++nunlabeled;
      
	       //Check the unlabeled event is in the execdata map
	       auto emit = exec_data->find(call.get_fid());
	       if(emit == exec_data->end()) std::cout << "WARNING: for event " << call.get_json().dump(2) << ", associated exec-data for fid " << call.get_fid() << " does not exist!" << std::endl;
	       else{
		 bool found = false;
		 for(auto const &cit : emit->second){
		   if(cit->get_id() == call.get_id()){
		     found = true;
		     break;
		   }
		 }
		 if(!found) std::cout << "WARNING: event " << call.get_json().dump(2) << " does not exist in exec-data!" << std::endl;
	       }
	     }
	     else if(call.reference_count()){
	       ++nlocked;

	       //If the stream has ended, output debug information on events that remain locked
	       if(stream_has_ended) std::cout << "WARNING, stream has ended and event remains locked: " << call.get_json().dump(2) << std::endl;
	       
	     }
	     else ++nunknown;
	   }
	 }
       }
     }   	        
     std::cout << "At step end, " << m_event->getCallListSize() << " events remain in the call list (unlabeled=" << nunlabeled << " locked=" << nlocked
	       << " incomplete=" << nincomplete << " unknown=" << nunknown << ") and " << cexec << " in the list of unlabeled entries" << std::endl;
   }
   
 }

 void Chimbuko::recordResetPeriodicPerfData(PerfPeriodic &perf_prd){
    //Write out how many events remain in the ExecData and how many unmatched correlation IDs there are
   auto const &purge_report = this->m_purge_report;
   perf_prd.add("call_list_purged", purge_report.n_purged);
   perf_prd.add("call_list_kept_protected", purge_report.n_kept_protected);
   perf_prd.add("call_list_kept_incomplete", purge_report.n_kept_incomplete);
   perf_prd.add("call_list_kept_window", purge_report.n_kept_window);
   perf_prd.add("call_list_kept_unlabeled", purge_report.n_kept_unlabeled);

   //Write accumulated event counts
   perf_prd.add("event_count_func", m_accum_prd.n_func_events);
   perf_prd.add("event_count_comm", m_accum_prd.n_comm_events);
   perf_prd.add("event_count_counter", m_accum_prd.n_counter_events);
    
   //m_perf_prd.add("call_list_carryover_size", m_event->getCallListSize());
   perf_prd.add("n_unmatched_correlation_id", m_event->getUnmatchCorrelationIDevents().size());
 }



void mainOptArgs(commandLineParser &parser, ChimbukoParams &into, int &input_data_rank){
  addOptionalCommandLineArgWithDefault(parser, into, func_threshold_file, "", "Provide the path to a file containing HBOS/COPOD algorithm threshold overrides for specified functions. Format is JSON: \"[ { \"fname\": <FUNC>, \"threshold\": <THRES> },... ]\". Empty string (default) uses default threshold for all funcs");
  addOptionalCommandLineArgWithDefault(parser, into, ignored_func_file, "", "Provide the path to a file containing function names (one per line) which the AD algorithm will ignore. All such events are labeled as normal. Empty string (default) performs AD on all events.");
  addOptionalCommandLineArgWithDefault(parser, into, anom_win_size, 10, "When anomaly data are recorded a window of this size (in units of function execution events) around the anomalous event are also recorded (default 10)");
  addOptionalCommandLineArgWithDefault(parser, into, trace_connect_timeout, 60, "(For SST mode) Set the timeout in seconds on the connection to the TAU-instrumented binary (default 60s)");
  addOptionalCommandLineArgWithDefault(parser, into, outlier_statistic, "exclusive_runtime", "Set the statistic used for outlier detection. Options: exclusive_runtime (default), inclusive_runtime");
  addOptionalCommandLineArgWithDefault(parser, into, prov_min_anom_time, 0, "Set the minimum exclusive runtime (in microseconds) for anomalies to recorded in the provenance output (default 0)");
  addOptionalCommandLineArgWithDefault(parser, into, monitoring_watchlist_file, "", "Provide a filename containing the counter watchlist for the integration with the monitoring plugin. Empty string (default) uses the default subset. File format is JSON: \"[ [<COUNTER NAME>, <FIELD NAME>], ... ]\" where COUNTER NAME is the name of the counter in the input data stream and FIELD NAME the name of the counter in the provenance output.");
  addOptionalCommandLineArgWithDefault(parser, into, monitoring_counter_prefix, "", "Provide an optional prefix marking a set of monitoring plugin counters to be captured, on top of or superseding the watchlist. Empty string (default) is ignored.");
  addOptionalCommandLineArgWithDefault(parser, into, parser_beginstep_timeout, 30, "Set the timeout in seconds on waiting for the next ADIOS2 timestep (default 30s)");
  parser.addOptionalArgWithFlag(input_data_rank, into.override_rank=false, "-override_rank", "Set Chimbuko to overwrite the rank index in the parsed data with its own internal rank parameter. The value provided should be the original rank index of the data. This disables verification of the data rank.");
}

void printHelp(){
  std::cout << "Usage: driver <Trace engine type> <Trace directory> <Trace file prefix> <Options>\n"
	    << "Where <Trace engine type> : BPFile or SST\n"
	    << "      <Trace directory>   : The directory in which the BPFile or SST file is located\n"
	    << "      <Trace file prefix> : The prefix of the file (the trace file name without extension e.g. \"tau-metrics-mybinary\" for \"tau-metrics-mybinary.bp\")\n"
	    << "      <Options>           : Optional arguments as described below.\n";
  commandLineParser p; ChimbukoParams pp; int r;
  setupBaseOptionalArgs(p, pp.base_params);
  mainOptArgs(p, pp, r);
  p.help(std::cout);
}

ChimbukoParams getParamsFromCommandLine(int argc, char** argv
#ifdef USE_MPI
, const int mpi_world_rank
#endif
){
  if(argc < 3){
    std::cerr << "Expected at least 3 arguments: <BPFile/SST> <.bp location> <bp file prefix>" << std::endl;
    exit(-1);
  }

  // -----------------------------------------------------------------------
  // Parse command line arguments (cf chimbuko.hpp for detailed description of parameters)
  // -----------------------------------------------------------------------
  ChimbukoParams params;

  //Parameters for the connection to the instrumented binary trace output
  params.trace_engineType = argv[0]; // BPFile or SST
  params.trace_data_dir = argv[1]; // *.bp location
  std::string bp_prefix = argv[2]; // bp file prefix (e.g. tau-metrics-[nwchem])

  commandLineParser parser;
  int input_data_rank;
  setupBaseOptionalArgs(parser, params.base_params);
  mainOptArgs(parser, params, input_data_rank);
  
  parser.parse(argc-3, (const char**)(argv+3));

  //By default assign the rank index of the trace data as the MPI rank of the AD process
  //Allow override by user
  if(params.base_params.rank < 0){
#ifdef USE_MPI
    params.base_params.rank = mpi_world_rank;
#else
    params.base_params.rank = 0; //default to 0 for non-MPI applications
#endif
  }

  params.base_params.verbose = params.base_params.rank == 0; //head node produces verbose output

  //Assume the rank index of the data is the same as the driver rank parameter
  params.trace_inputFile = bp_prefix + "-" + std::to_string(params.base_params.rank) + ".bp";

  //If we are forcing the parsed data rank to match the driver rank parameter, this implies it was not originally
  //Thus we need to obtain the input data rank also from the command line and modify the filename accordingly
  if(params.override_rank)
    params.trace_inputFile = bp_prefix + "-" + std::to_string(input_data_rank) + ".bp";

  //If neither the provenance database or the provenance output path are set, default to outputting to pwd
  if(params.base_params.prov_outputpath.size() == 0
#ifdef ENABLE_PROVDB
     && params.base_params.provdb_addr_dir.size() == 0
#endif
     ){
    params.base_params.prov_outputpath = ".";
  }

  return params;
}


std::unique_ptr<ChimbukoBase> chimbuko::modules::performance_analysis::moduleInstantiateChimbuko(int argc, char ** argv){
  if(argc == 0 || (argc == 1 && std::string(argv[0]) == "-help") ){
    printHelp();
    return 0;
  }

#ifdef USE_MPI
  int mpi_world_rank, mpi_world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_world_size);
#endif

  //Parse Chimbuko parameters
  ChimbukoParams params = getParamsFromCommandLine(argc, argv
#ifdef USE_MPI
    , mpi_world_rank
#endif
    );

  if(params.base_params.rank == progressHeadRank()) params.print();

  headProgressStream(params.base_params.rank) << "Initializing PerformanceAnalysis module" << std::endl;

  return std::unique_ptr<ChimbukoBase>(new Chimbuko(params));
}
