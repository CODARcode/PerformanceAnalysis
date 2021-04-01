#include "chimbuko/ad/ADOutlier.hpp"
#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/param/hbos_param.hpp"
#include "chimbuko/message.hpp"
#include "chimbuko/verbose.hpp"
#include "chimbuko/util/error.hpp"
#include <mpi.h>
#include <nlohmann/json.hpp>

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

void ADOutlier::linkNetworkClient(ADNetClient *client){
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
ADOutlierSSTD::ADOutlierSSTD(OutlierStatistic stat, double sigma) : ADOutlier(stat), m_sigma(sigma) { //m_sigma(6.0)
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
        param[func_id].create_histogram(runtimes);
      }
      else { //merge with exisiting func_id, not overwrite

        param[func_id].merge_histograms(g[func_id], runtimes);
      }
    }
  }

  //Update temp runstats to include information collected previously (synchronizes with the parameter server if connected)
  PerfTimer timer;
  timer.start();
  std::pair<size_t, size_t> msgsz = sync_param(&param);

  if(m_perf != nullptr){
    m_perf->add("param_update_us", timer.elapsed_us());
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

  //Display global bin_edges
  //std::vector<double> tmp_b_edges = param[func_id].bin_edges();
  //std::cout << "global bin_edges in compute_outliers: Size: " << tmp_b_edges.size() << std::endl;
  //for(int i=0; i<tmp_b_edges.size(); i++){
  //  std::cout << tmp_b_edges.at(i) << std::endl;
  //}

  //if (param[func_id].count() < 2){
  //  VERBOSE(std::cout << "Less than 2 events in stats associated with that func, stats not complete" << std::endl);
  //  return 0;
  //}
  unsigned long n_outliers = 0;

  //probability of runtime counts
  std::vector<double> prob_counts = std::vector<double>(param[func_id].counts().size(), 0.0);
  double tot_runtimes = std::accumulate(param[func_id].counts().begin(), param[func_id].counts().end(), 0.0);
  //std::cout << "Count and its Probability for func_id: " << std::to_string(func_id) << std::endl;
  for(int i=0; i < param[func_id].counts().size(); i++){
    int count = param[func_id].counts().at(i);
    double p = count / tot_runtimes;
    prob_counts.at(i) += p;
    //std::cout << "Count: " << count << ", Probability: " << prob_counts.at(i) << std::endl;
  }

  //Create HBOS score vector
  std::vector<double> out_scores_i;
  double min_score = -1 * log2(0.0 + m_alpha);
  double max_score = -1 * log2(1.0 + m_alpha);
  verboseStream << "out_scores_i: " << std::endl;
  for(int i=0; i < prob_counts.size(); i++){
    double l = -1 * log2(prob_counts.at(i) + m_alpha);
    out_scores_i.push_back(l);
    verboseStream << "Probability: " << prob_counts.at(i) << ", score: "<< l << std::endl;
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
  // std::cout << "out_score_i size: " << out_scores_i.size() << std::endl;
  // std::cout << "min_score = " << min_score << std::endl;
  // std::cout << "max_score = " << max_score << std::endl;
  if (out_scores_i.size() == 0) return 0;

  //compute threshold
  //std::cout << "Global threshold before comparison with local threshold =  " << param[func_id].get_threshold() << std::endl;
  double l_threshold = min_score + (m_threshold * (max_score - min_score));//m_threshold * max_score;
  if(m_use_global_threshold) {
    if(l_threshold < param[func_id].get_threshold()) {
      l_threshold = param[func_id].get_threshold();
    } else {
      param[func_id].set_glob_threshold(l_threshold); //.get_histogram().glob_threshold = l_threshold;
      //std::pair<size_t, size_t> msgsz_thres_update = sync_param(&param);
    }
  }

  //std::cout << "local threshold = " << l_threshold << " updated global_threshold = " << param[func_id].get_threshold() << std::endl;
  // For each datapoint get its corresponding bin index
  //std::vector<int> bin_inds = ADOutlierHBOS::np_digitize(param[func_id].runtimes, param[func_id].bin_edges);
  //if (bin_inds.size() < param[func_id].runtimes.size()) {
  //  VERBOSE(std::cout << "INCORRECT bin_inds.size() < param[func_id].runtimes.size()\t: " << bin_inds.size() << " < " << param[func_id].runtimes.size() << std::endl);
  //  return 0;
  //}

  //Compute HBOS based score for each datapoint
  const double bin_width = param[func_id].bin_edges().at(1) - param[func_id].bin_edges().at(0);
  const int num_bins = param[func_id].counts().size();
  // std::cout << "Bin width: " << bin_width << std::endl;
  // std::cout << "Bin edges: " << std::endl;
  //for (int i=0; i< param[func_id].bin_edges().size(); i++){
    //std::cout << param[func_id].bin_edges().at(i) << std::endl;
  //}

  int top_out = 0;
  for (auto itt : data) {
    if (itt->get_label() == 0) {

      const double runtime_i = this->getStatisticValue(*itt); //runtimes.push_back(this->getStatisticValue(*itt));
      double ad_score;
      // auto bin_it = std::upper_bound(param[func_id].bin_edges().begin(), param[func_id].bin_edges().end(), runtime_i);
      // if(bin_it == param[func_id].bin_edges().end()) {// Not in histogram
      //   ad_score = max_score;
      // }
      // else{ //Found in Histogram
      //   const int index = std::distance(param[func_id].bin_edges().begin(), bin_it);
      //   ad_score = out_scores_i.at(index);
      // }
      // int bin_ind = ADOutlierHBOS::np_digitize_get_bin_inds(runtime_i, param[func_id].bin_edges());
      int bin_ind = std::distance(param[func_id].bin_edges().begin(), std::upper_bound(param[func_id].bin_edges().begin(), param[func_id].bin_edges().end(), runtime_i)) - 1;
      verboseStream << "bin_ind: " << bin_ind << " for runtime_i: " << runtime_i << std::endl;
      // If the sample does not belong to any bins
      // bin_ind == 0 (fall outside since it is too small)
      if( bin_ind == 0){
        double first_bin_edge = param[func_id].bin_edges().at(0);
        double dist = first_bin_edge - runtime_i;
        if( dist <= (bin_width * 0.05) ){
          verboseStream << runtime_i << " is small but NOT outlier" << std::endl;
          ad_score = out_scores_i.at(0);
        }
        else{
          verboseStream << runtime_i << " is small and outlier" << std::endl;
          ad_score = max_score;
        }
        //std::cout << "bin_index=0: Anomaly score of " << runtime_i << " = " << ad_score <<std::endl;
      }
      // If the sample does not belong to any bins
      else if(bin_ind == num_bins + 1){
        int last_idx = param[func_id].bin_edges().size() - 1;
        double last_bin_edge = param[func_id].bin_edges().at(last_idx);
        double dist = runtime_i - last_bin_edge;

        if (dist <= (bin_width * 0.05)) {
          verboseStream << runtime_i << " is large but NOT outlier" << std::endl;
          ad_score = out_scores_i.at(num_bins - 1);
        }
        else{
          verboseStream << runtime_i << " is large and outlier" << std::endl;
          ad_score = max_score;
        }
        //std::cout << "bin_index=num_bins+1: Anomaly score of " << runtime_i << " = " << ad_score <<std::endl;
      }
      else {
        verboseStream << runtime_i << " can be an outlier" << std::endl;
        ad_score = out_scores_i.at( bin_ind - 1);
        //std::cout << "Anomaly score of " << runtime_i << " = " << ad_score <<std::endl;
      }

      verboseStream << "ad_score: " << ad_score << ", l_threshold: " << l_threshold << std::endl;
      //if(ad_score != (-1 * log2(0.0 + m_alpha))) {
      //Compare the ad_score with the threshold
      if (ad_score >= l_threshold) {

          itt->set_label(-1);
          verboseStream << "!!!!!!!Detected outlier on func id " << func_id << " (" << itt->get_funcname() << ") on thread " << itt->get_tid() << " runtime " << runtime_i << std::endl;
          outliers.insert(itt, Anomalies::EventType::Outlier, runtime_i, ad_score, l_threshold); //insert into data structure containing captured anomalies
          n_outliers += 1;

      }
    //}
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

  int ret_val = bin_edges.size();

  return  ret_val;
}
