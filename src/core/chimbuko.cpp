#include <limits>
#include "chimbuko/core/chimbuko.hpp"
#include "chimbuko/core/verbose.hpp"
#include "chimbuko/core/util/error.hpp"
#include "chimbuko/core/util/time.hpp"
#include "chimbuko/core/util/memutils.hpp"
#include "chimbuko/core/ad/utils.hpp"

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
    if(m_base_params.hpserver_nthr <= 0) throw std::runtime_error("ChimbukoBase::init_net_client Input parameter hpserver_nthr cannot be <1");
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

void ChimbukoBase::sendPSdata(bool force){
  if(m_net_client && m_net_client->use_ps() && ( (m_step + this->getRank()) % m_base_params.ps_send_stats_freq == 0 || force) ){ //stagger sends by offsetting by rank
    this->sendPSdataImplementation(*m_net_client, m_step);
  }
}

bool ChimbukoBase::doStepReport(int step) const{
  return enableVerboseLogging() || (m_base_params.step_report_freq > 0 && step % m_base_params.step_report_freq == 0);
}
bool ChimbukoBase::doAnalysisOnStep(int step) const{
  return step % m_base_params.analysis_step_freq == 0;
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
