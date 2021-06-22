#include "chimbuko/ad/ADOutlier.hpp"
#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/param/hbos_param.hpp"
#include "chimbuko/message.hpp"
#include "chimbuko/verbose.hpp"
#include "chimbuko/util/error.hpp"
#include <mpi.h>
#include <nlohmann/json.hpp>
#include <boost/math/distributions/normal.hpp>

using namespace chimbuko;

/* ---------------------------------------------------------------------------
 * Implementation of ADOutlier class
 * --------------------------------------------------------------------------- */
ADOutlier::ADOutlier(OutlierStatistic stat)
  : m_execDataMap(nullptr), m_param(nullptr), m_use_ps(false), m_perf(nullptr), m_statistic(stat)
{
}

ADOutlier::~ADOutlier() {
    if (m_param) {
        delete m_param;
    }
}

ADOutlier *ADOutlier::set_algorithm(OutlierStatistic stat, const std::string & algorithm, const double & hbos_thres, const bool & glob_thres, const double & sstd_sigma) {

  if (algorithm == "sstd" || algorithm == "SSTD") {

    return new ADOutlierSSTD(stat, sstd_sigma);
  }
  else if (algorithm == "hbos" || algorithm == "HBOS") {
    return new ADOutlierHBOS(stat, hbos_thres, glob_thres);
  }
  else {
    return nullptr;
  }
}

void ADOutlier::linkNetworkClient(ADThreadNetClient *client){
  m_net_client = client;
  m_use_ps = (m_net_client != nullptr && m_net_client->use_ps());
}

double ADOutlier::getStatisticValue(const ExecData_t &e) const{
  switch(m_statistic){
  case ExclusiveRuntime:
    return e.get_exclusive();
  case InclusiveRuntime:
    return e.get_inclusive();
  default:
    throw std::runtime_error("Invalid statistic");
  }
}

/* ---------------------------------------------------------------------------
 * Implementation of ADOutlierSSTD class
 * --------------------------------------------------------------------------- */
ADOutlierSSTD::ADOutlierSSTD(OutlierStatistic stat, double sigma) : ADOutlier(stat), m_sigma(sigma) {
    m_param = new SstdParam();
}

ADOutlierSSTD::~ADOutlierSSTD() {
}

std::pair<size_t,size_t> ADOutlierSSTD::sync_param(ParamInterface const* param)
{
  SstdParam& g = *(SstdParam*)m_param; //global parameter set
  const SstdParam & l = *(SstdParam const*)param; //local parameter set

    if (!m_use_ps) {
        g.update(l);
        return std::make_pair(0, 0);
    }
    else {
        Message msg;
        msg.set_info(m_net_client->get_client_rank(), m_net_client->get_server_rank(), MessageType::REQ_ADD, MessageKind::PARAMETERS);
        msg.set_msg(l.serialize(), false);
        size_t sent_sz = msg.size();

	m_net_client->send_and_receive(msg, msg);
        size_t recv_sz = msg.size();
        g.assign(msg.buf());
        return std::make_pair(sent_sz, recv_sz);
    }
}

Anomalies ADOutlierSSTD::run(int step) {
  Anomalies outliers;
  if (m_execDataMap == nullptr) return outliers;

  //If using CUDA without precompiled kernels the first time a function is encountered takes much longer as it does a JIT compile
  //Python scripts also appear to take longer executing a function the first time
  //This is worked around by ignoring the first time a function is encountered (per device)
  //Set this environment variable to disable the workaround
  bool cuda_jit_workaround = true;
  if(const char* env_p = std::getenv("CHIMBUKO_DISABLE_CUDA_JIT_WORKAROUND")){
    cuda_jit_workaround = false;
  }

  //Generate the statistics based on this IO step
  SstdParam param;
  for (auto it : *m_execDataMap) { //loop over functions (key is function index)
    unsigned long func_id = it.first;
    for (auto itt : it.second) { //loop over events for that function
	if(itt->get_label() == 0){ //has not been analyzed previously
	//Update local counts of number of times encountered
	std::array<unsigned long, 4> fkey({itt->get_pid(), itt->get_rid(), itt->get_tid(), func_id});
	auto encounter_it = m_local_func_exec_count.find(fkey);
	if(encounter_it == m_local_func_exec_count.end())
	  encounter_it = m_local_func_exec_count.insert({fkey, 0}).first;
	else
	  encounter_it->second++;

	if(!cuda_jit_workaround || encounter_it->second > 0){ //ignore first encounter to avoid including CUDA JIT compiles in stats (later this should be done only for GPU kernels
	  param[func_id].push( this->getStatisticValue(*itt) );
	}
      }
    }
  }

  //Update temp runstats to include information collected previously (synchronizes with the parameter server if connected)
  PerfTimer timer;
  timer.start();
  std::pair<size_t, size_t> msgsz = sync_param(&param);

  if(m_perf != nullptr){
    m_perf->add("param_update_ms", timer.elapsed_ms());
    m_perf->add("param_sent_MB", (double)msgsz.first / 1000000.0); // MB
    m_perf->add("param_recv_MB", (double)msgsz.second / 1000000.0); // MB
  }

  //Run anomaly detection algorithm
  for (auto it : *m_execDataMap) { //loop over function index
    const unsigned long func_id = it.first;
    const unsigned long n = compute_outliers(outliers,func_id, it.second);
  }
  return outliers;
}


double ADOutlierSSTD::computeScore(CallListIterator_t ev, const SstdParam &stats) const{
  auto it = stats.get_runstats().find(ev->get_fid());
  if(it == stats.get_runstats().end()) fatal_error("Function not in stats!");
  double runtime = this->getStatisticValue(*ev);
  double mean = it->second.mean();
  double std_dev = it->second.stddev();
  if(std_dev == 0.) std_dev = 1e-10; //distribution throws an error if std.dev = 0
  boost::math::normal_distribution<double> dist(mean, std_dev);
  double cdf_val = boost::math::cdf(dist, runtime); // P( X <= x ) for random variable X
  double score = std::min(cdf_val, 1-cdf_val); //two-tailed
  verboseStream << "ADOutlierSSTD::computeScore " << ev->get_funcname() << " runtime " << runtime << " mean " << mean << " std.dev " << std_dev << " -> score " << score << std::endl;
  return score;
}


unsigned long ADOutlierSSTD::compute_outliers(Anomalies &outliers,
					      const unsigned long func_id,
					      std::vector<CallListIterator_t>& data){
  verboseStream << "Finding outliers in events for func " << func_id << std::endl;


  SstdParam& param = *(SstdParam*)m_param;
  if (param[func_id].count() < 2){
    verboseStream << "Less than 2 events in stats associated with that func, stats not complete" << std::endl;
    return 0;
  }
  unsigned long n_outliers = 0;

  const double mean = param[func_id].mean();
  const double std = param[func_id].stddev();

  const double thr_hi = mean + m_sigma * std;
  const double thr_lo = mean - m_sigma * std;

  for (auto itt : data) {
    if(itt->get_label() == 0){ //only label new events
      const double runtime = this->getStatisticValue(*itt);
      int label = (thr_lo > runtime || thr_hi < runtime) ? -1: 1;
      itt->set_label(label);
      itt->set_outlier_score(computeScore(itt, param));

      if (label == -1) {
	verboseStream << "!!!!!!!Detected outlier on func id " << func_id << " (" << itt->get_funcname() << ") on thread " << itt->get_tid()
		      << " runtime " << runtime << " mean " << mean << " std " << std << std::endl;
	n_outliers += 1;
  	std::vector<double> sstd_stats{thr_hi, thr_lo, mean, std};
	outliers.insert(itt, Anomalies::EventType::Outlier, sstd_stats); //insert into data structure containing captured anomalies
      }else{
	//Capture maximum of one normal execution per io step
	if(outliers.nFuncEvents(func_id, Anomalies::EventType::Normal) == 0){
	  verboseStream << "Detected normal event on func id " << func_id << " (" << itt->get_funcname() << ") on thread " << itt->get_tid()
			<< " runtime " << runtime << " mean " << mean << " std " << std << std::endl;

	  outliers.insert(itt, Anomalies::EventType::Normal);
	}
      }
    }
  }

  return n_outliers;
}




/* ---------------------------------------------------------------------------
 * Implementation of ADOutlierHBOS class
 * --------------------------------------------------------------------------- */
ADOutlierHBOS::ADOutlierHBOS(OutlierStatistic stat, double threshold, bool use_global_threshold) : ADOutlier(stat), m_alpha(0.00001), m_threshold(threshold), m_use_global_threshold(use_global_threshold) {
    m_param = new HbosParam();
}

ADOutlierHBOS::~ADOutlierHBOS() {
  if (m_param)
    m_param->clear();
}

std::pair<size_t,size_t> ADOutlierHBOS::sync_param(ParamInterface const* param)
{
  HbosParam& g = *(HbosParam*)m_param; //global parameter set
  const HbosParam & l = *(HbosParam const*)param; //local parameter set

    if (!m_use_ps) {
        verboseStream << "m_use_ps not USED!" << std::endl;
        g.update(l);
        return std::make_pair(0, 0);
    }
    else {
        Message msg;
        msg.set_info(m_net_client->get_client_rank(), m_net_client->get_server_rank(), MessageType::REQ_ADD, MessageKind::PARAMETERS);
        msg.set_msg(l.serialize(), false);
        size_t sent_sz = msg.size();
   
        m_net_client->send_and_receive(msg, msg);
        size_t recv_sz = msg.size();
        g.assign(msg.buf());
        return std::make_pair(sent_sz, recv_sz);
    }
}

Anomalies ADOutlierHBOS::run(int step) {
  Anomalies outliers;
  if (m_execDataMap == nullptr) return outliers;

  //If using CUDA without precompiled kernels the first time a function is encountered takes much longer as it does a JIT compile
  //Python scripts also appear to take longer executing a function the first time
  //This is worked around by ignoring the first time a function is encountered (per device)
  //Set this environment variable to disable the workaround
  bool cuda_jit_workaround = true;
  if(const char* env_p = std::getenv("CHIMBUKO_DISABLE_CUDA_JIT_WORKAROUND")){
    cuda_jit_workaround = false;
  }

  //Generate the statistics based on this IO step
  HbosParam param;
  HbosParam& g = *(HbosParam*)m_param;
  for (auto it : *m_execDataMap) { //loop over functions (key is function index)
    unsigned long func_id = it.first;
    std::vector<double> runtimes;
    for (auto itt : it.second) { //loop over events for that function
      if (itt->get_label() == 0) {
        //Update local counts of number of times encountered
        std::array<unsigned long, 4> fkey({itt->get_pid(), itt->get_rid(), itt->get_tid(), func_id});
        auto encounter_it = m_local_func_exec_count.find(fkey);
        if(encounter_it == m_local_func_exec_count.end())
  	encounter_it = m_local_func_exec_count.insert({fkey, 0}).first;
        else
  	encounter_it->second++;

        if(!cuda_jit_workaround || encounter_it->second > 0){ //ignore first encounter to avoid including CUDA JIT compiles in stats (later this should be done only for GPU kernels

           runtimes.push_back(this->getStatisticValue(*itt));
        }
      }
    }
    if (runtimes.size() > 0) {
      if (!g.find(func_id)) { // If func_id does not exist
        const int r = param[func_id].create_histogram(runtimes);
        if (r < 0) {return outliers;}
      }
      else { //merge with exisiting func_id, not overwrite

        const int r = param[func_id].merge_histograms(g[func_id], runtimes);
	if (r < 0) {return outliers;}
      }
    }
    else { return outliers;}
  }

  //Update temp runstats to include information collected previously (synchronizes with the parameter server if connected)
  PerfTimer timer;
  timer.start();
  std::pair<size_t, size_t> msgsz = sync_param(&param);

  if(m_perf != nullptr){
    m_perf->add("param_update_ms", timer.elapsed_ms());
    m_perf->add("param_sent_MB", (double)msgsz.first / 1000000.0); // MB
    m_perf->add("param_recv_MB", (double)msgsz.second / 1000000.0); // MB
  }

  //Run anomaly detection algorithm
  for (auto it : *m_execDataMap) { //loop over function index
    const unsigned long func_id = it.first;
    const unsigned long n = compute_outliers(outliers,func_id, it.second);
  }

  return outliers;
}

unsigned long ADOutlierHBOS::compute_outliers(Anomalies &outliers,
					      const unsigned long func_id,
					      std::vector<CallListIterator_t>& data){

  verboseStream << "Finding outliers in events for func " << func_id << std::endl;

  HbosParam& param = *(HbosParam*)m_param;


  unsigned long n_outliers = 0;

  //probability of runtime counts
  std::vector<double> prob_counts = std::vector<double>(param[func_id].counts().size(), 0.0);
  double tot_runtimes = std::accumulate(param[func_id].counts().begin(), param[func_id].counts().end(), 0.0);

  for(int i=0; i < param[func_id].counts().size(); i++){
    int count = param[func_id].counts().at(i);
    double p = count / tot_runtimes;
    prob_counts.at(i) += p;

  }

  //Create HBOS score vector
  std::vector<double> out_scores_i;
  double min_score = -1 * log2(0.0 + m_alpha);
  double max_score = -1 * log2(1.0 + m_alpha);
  verboseStream << "out_scores_i: " << std::endl;
  for(int i=0; i < prob_counts.size(); i++){
    double l = -1 * log2(prob_counts.at(i) + m_alpha);
    out_scores_i.push_back(l);
    verboseStream << "Count: " << param[func_id].counts().at(i) << ", Probability: " << prob_counts.at(i) << ", score: "<< l << std::endl;
    if(prob_counts.at(i) > 0) {
      if(l < min_score){
        min_score = l;
      }
      if(l > max_score){
        max_score = l;
      }
    }
  }
  verboseStream << std::endl;
  verboseStream << "out_score_i size: " << out_scores_i.size() << std::endl;
  verboseStream << "min_score = " << min_score << std::endl;
  verboseStream << "max_score = " << max_score << std::endl;

  if (out_scores_i.size() <= 0) {return 0;}

  //compute threshold
  verboseStream << "Global threshold before comparison with local threshold =  " << param[func_id].get_threshold() << std::endl;
  double l_threshold = min_score + (m_threshold * (max_score - min_score));
  if(m_use_global_threshold) {
    if(l_threshold < param[func_id].get_threshold()) {
      l_threshold = param[func_id].get_threshold();
    } else {
      param[func_id].set_glob_threshold(l_threshold); //.get_histogram().glob_threshold = l_threshold;
      //std::pair<size_t, size_t> msgsz_thres_update = sync_param(&param);
    }
  }

  //Compute HBOS based score for each datapoint
  const double bin_width = param[func_id].bin_edges().at(1) - param[func_id].bin_edges().at(0);
  const int num_bins = param[func_id].counts().size();
  verboseStream << "Bin width: " << bin_width << std::endl;

  int top_out = 0;
  for (auto itt : data) {
    if (itt->get_label() == 0) {

      const double runtime_i = this->getStatisticValue(*itt); //runtimes.push_back(this->getStatisticValue(*itt));
      double ad_score;

      const int bin_ind = ADOutlierHBOS::np_digitize_get_bin_inds(runtime_i, param[func_id].bin_edges());
      verboseStream << "bin_ind: " << bin_ind << " for runtime_i: " << runtime_i << ", where bin_edges Size:" << param[func_id].bin_edges().size() << " & num_bins: "<< num_bins << std::endl;
      /**
       * If the sample does not belong to any bins
       * bin_ind == 0 (fall outside since it is too small)
       */
      if( bin_ind == 0){
        const double first_bin_edge = param[func_id].bin_edges().at(0);
        const double dist = first_bin_edge - runtime_i;
	verboseStream << "First_bin_edge: " << first_bin_edge << std::endl;
        if( dist <= (bin_width * 0.05) ){
          verboseStream << runtime_i << " is on left of histogram but NOT outlier" << std::endl;
	  if(param[func_id].counts().size() < 1) {return 0;}
          if(param[func_id].counts().at(0) == 0) { /**< Ignore zero counts */
	    
            ad_score = l_threshold - 1;
            verboseStream << "corrected ad_score: " << ad_score << std::endl;
          }
          else {
            ad_score = out_scores_i.at(0);
	    verboseStream << "ad_score: " << ad_score << std::endl;
          }
        }
        else{
          verboseStream << runtime_i << " is on left of histogram and an outlier" << std::endl;
          ad_score = max_score;
	  verboseStream << "ad_score(max_score): " << ad_score << std::endl;
        }

      }
      /**
       *  If the sample does not belong to any bins
       */
      else if(bin_ind == num_bins + 1){
        const int last_idx = param[func_id].bin_edges().size() - 1;
        const double last_bin_edge = param[func_id].bin_edges().at(last_idx);
        const double dist = runtime_i - last_bin_edge;
	verboseStream << "last_indx: " << last_idx << ", last_bin_edge: " << last_bin_edge << std::endl;
        if (dist <= (bin_width * 0.05)) {
          if(param[func_id].counts().at(num_bins - 1) == 0) {   //bin_ind) == 0) {  /**< Ignore zero counts */

            ad_score = l_threshold - 1;
            verboseStream << "corrected ad_score: " << ad_score << std::endl;
          }
          else {
            verboseStream << runtime_i << " is on right of histogram but NOT outlier" << std::endl;
            ad_score = out_scores_i.at(num_bins - 1);
	    verboseStream << "ad_score: " << ad_score << ", num_bins: " << num_bins << ", out_scores_i size: " << out_scores_i.size() << std::endl;
          }
        }
        else{
          verboseStream << runtime_i << " is on right of histogram and an outlier" << std::endl;
          ad_score = max_score;
	  verboseStream << "ad_score(max_score): " << ad_score << ", num_bins: " << num_bins << ", out_scores_i size: " << out_scores_i.size() << std::endl;
        }

      }
      else {

        if(param[func_id].counts().at(bin_ind) == 0) { /**< Ignore zero counts */

          ad_score = l_threshold - 1;
          verboseStream << "corrected ad_score: " << ad_score << std::endl;
        }
        else {
          verboseStream << runtime_i << " maybe be an outlier" << std::endl;
          ad_score = out_scores_i.at( bin_ind - 1);
	  verboseStream << "ad_score(else): " << ad_score << ", bin_ind: " << bin_ind  << ", num_bins: " << num_bins << ", out_scores_i size: " << out_scores_i.size() << std::endl;
        }

      }

      itt->set_outlier_score(ad_score);
      verboseStream << "ad_score: " << ad_score << ", l_threshold: " << l_threshold << std::endl;

      //Compare the ad_score with the threshold
      if (ad_score >= l_threshold) {

          itt->set_label(-1);
          verboseStream << "!!!!!!!Detected outlier on func id " << func_id << " (" << itt->get_funcname() << ") on thread " << itt->get_tid() << " runtime " << runtime_i << std::endl;
          outliers.insert(itt, Anomalies::EventType::Outlier, runtime_i, ad_score, l_threshold); //insert into data structure containing captured anomalies
          n_outliers += 1;

      }
      else {
        //Capture maximum of one normal execution per io step
        itt->set_label(1);
        if(outliers.nFuncEvents(func_id, Anomalies::EventType::Normal) == 0) {
      	   verboseStream << "Detected normal event on func id " << func_id << " (" << itt->get_funcname() << ") on thread " << itt->get_tid() << " runtime " << runtime_i << std::endl;
      	   outliers.insert(itt, Anomalies::EventType::Normal);

        }

      }
    }
  }

  return n_outliers;
}


int ADOutlierHBOS::np_digitize_get_bin_inds(const double& X, const std::vector<double>& bin_edges) {


  if(bin_edges.size() < 2){ // If only one bin exists in the Histogram
    return 0;
  }


  for(int j=1; j < bin_edges.size(); j++){

    if(X <= bin_edges.at(j)){

      return (j-1);
    }
  }

  const int ret_val = bin_edges.size();

  return  ret_val;
}
