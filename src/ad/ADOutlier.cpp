#include "chimbuko/ad/ADOutlier.hpp"
#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/param/hbos_param.hpp"
#include "chimbuko/param/copod_param.hpp"
#include "chimbuko/message.hpp"
#include "chimbuko/verbose.hpp"
#include "chimbuko/util/error.hpp"
#include <mpi.h>
#include <nlohmann/json.hpp>
#include <boost/math/distributions/normal.hpp>
#include <boost/math/distributions/empirical_cumulative_distribution_function.hpp>
#include <limits>

using namespace chimbuko;

ADOutlier::AlgoParams::AlgoParams(): stat(ADOutlier::ExclusiveRuntime), sstd_sigma(6.0), hbos_thres(0.99), glob_thres(true), hbos_max_bins(200), func_threshold_file(""){}


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


template<typename ADOutlierType>
void loadPerFunctionThresholds(ADOutlierType* into, const std::string &filename){
  if(filename == "") return;
  std::ifstream in(filename);
  if(!in.good()) fatal_error("Could not open function threshold file: " + filename);
  nlohmann::json j;
  in >> j;
  if(in.fail()) fatal_error("Error reading from function threshold file");    
  if(!j.is_array()) fatal_error("Function threshold file should be a json array");
  for(auto it = j.begin(); it != j.end(); it++){
    if(!it->contains("fname") || !it->contains("threshold")) fatal_error("Function threshold file invalid format");
    const std::string &fname = (*it)["fname"];
    double thres = (*it)["threshold"];
    progressStream << "Set per-function threshold: \"" << fname << "\" " << thres << std::endl;
    into->overrideFuncThreshold(fname, thres);
  }
}


ADOutlier *ADOutlier::set_algorithm(const std::string & algorithm, const AlgoParams &params) {
  if (algorithm == "sstd" || algorithm == "SSTD") {
    return new ADOutlierSSTD(params.stat, params.sstd_sigma);
  }
  else if (algorithm == "hbos" || algorithm == "HBOS") {
    ADOutlierHBOS* alg = new ADOutlierHBOS(params.stat, params.hbos_thres, params.glob_thres, params.hbos_max_bins);
    loadPerFunctionThresholds(alg,params.func_threshold_file);
    return alg;
  }
  else if (algorithm == "copod" || algorithm == "COPOD") {
    ADOutlierCOPOD* alg = new ADOutlierCOPOD(params.stat, params.hbos_thres);
    loadPerFunctionThresholds(alg,params.func_threshold_file);
    return alg;   
  }
  else{
    fatal_error("Invalid algorithm: " + algorithm);
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

std::pair<size_t,size_t> ADOutlier::sync_param(ParamInterface const* param)
{
  if (!m_use_ps) {
    verboseStream << "m_use_ps not USED!" << std::endl;
    m_param->update(param->serialize());
    return std::make_pair(0, 0);
  }
  else {
    Message msg;
    msg.set_info(m_net_client->get_client_rank(), m_net_client->get_server_rank(), MessageType::REQ_ADD, MessageKind::PARAMETERS);
    msg.set_msg(param->serialize(), false);
    size_t sent_sz = msg.size();

    m_net_client->send_and_receive(msg, msg);
    size_t recv_sz = msg.size();
    m_param->assign(msg.buf());
    return std::make_pair(sent_sz, recv_sz);
  }
}


bool ADOutlier::ignoringFunction(const std::string &func) const{
  return m_func_ignore.count(func) != 0;
}

void ADOutlier::setIgnoreFunction(const std::string &func){
  m_func_ignore.insert(func);
}

/* ---------------------------------------------------------------------------
 * Implementation of ADOutlierSSTD class
 * --------------------------------------------------------------------------- */
ADOutlierSSTD::ADOutlierSSTD(OutlierStatistic stat, double sigma) : ADOutlier(stat), m_sigma(sigma) {
    m_param = new SstdParam();
}

ADOutlierSSTD::~ADOutlierSSTD() {
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

  //boost::math::normal_distribution<double> dist(mean, std_dev);
  //double cdf_val = boost::math::cdf(dist, runtime); // P( X <= x ) for random variable X
  //double score = std::min(cdf_val, 1-cdf_val); //two-tailed

  //Using the CDF gives scores ~0 for basically any global outlier
  //Instead we will use the difference in runtime compared to the avg in units of the standard deviation
  double score = fabs( runtime - mean ) / std_dev;

  verboseStream << "ADOutlierSSTD::computeScore " << ev->get_funcname() << " runtime " << runtime << " mean " << mean << " std.dev " << std_dev << " -> score " << score << std::endl;
  return score;
}


unsigned long ADOutlierSSTD::compute_outliers(Anomalies &outliers,
					      const unsigned long func_id,
					      std::vector<CallListIterator_t>& data){
  verboseStream << "Finding outliers in events for func " << func_id << std::endl;

  if(data.size() == 0) return 0;

  const std::string &fname = data.front()->get_funcname();

  //Check whether we are ignoring this function
  if(ignoringFunction(fname)){
    verboseStream << "Ignoring function \"" << fname << "\"" << std::endl;
    for (auto itt : data) {
      if(itt->get_label() == 0)
	itt->set_label(1); //label as normal event
    }
    return 0;
  }
  
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
	outliers.insert(itt, Anomalies::EventType::Outlier); //insert into data structure containing captured anomalies
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
ADOutlierHBOS::ADOutlierHBOS(OutlierStatistic stat, double threshold, bool use_global_threshold, int maxbins) : ADOutlier(stat), m_alpha(78.88e-32), m_threshold(threshold), m_use_global_threshold(use_global_threshold), m_maxbins(maxbins) {
    m_param = new HbosParam();
}

ADOutlierHBOS::~ADOutlierHBOS() {
  if (m_param)
    m_param->clear();
}

double ADOutlierHBOS::getFunctionThreshold(const std::string &fname) const{
  double hbos_threshold = m_threshold; //default threshold
  //check to see if we have an override
  auto it = m_func_threshold_override.find(fname);
  if(it != m_func_threshold_override.end())
    hbos_threshold = it->second;
  return hbos_threshold;
}


Anomalies ADOutlierHBOS::run(int step) {
  Anomalies outliers;
  if (m_execDataMap == nullptr) return outliers;

  //Generate the statistics based on this IO step
  const HbosParam& global_param = *(HbosParam const*)m_param;
  HbosParam param;
  param.setMaxBins(m_maxbins); //communicates the maxbins parameter to the pserver (it should be the same for all clients)

  for (auto it : *m_execDataMap) { //loop over functions (key is function index)
    unsigned long func_id = it.first;
    std::vector<double> runtimes;
    for (auto itt : it.second) { //loop over events for that function
      if (itt->get_label() == 0)
	runtimes.push_back(this->getStatisticValue(*itt));
    }
    verboseStream << "Function " << func_id << " has " << runtimes.size() << " unlabeled data points of " << it.second.size() << std::endl;

    param.generate_histogram(func_id, runtimes, 0, &global_param); //initialize global threshold to 0 so that it is overridden by the merge
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

  //Check if there are any unlabeled data points first before we go through the effort of precomputing scores
  // size_t n_unlabeled = 0;
  // for (auto itt : data)
  //   if(itt->get_label() == 0) ++n_unlabeled;

  // if(n_unlabeled == 0){
  //   verboseStream << "Function has no unlabeled events, skipping" << std::endl;
  //   return 0;
  // }
  
  if(data.size() == 0) return 0;
  const std::string &fname = data.front()->get_funcname();
  verboseStream << "Function name is \"" << fname << "\"" << std::endl;

  //Check whether we are ignoring this function
  if(ignoringFunction(fname)){
    verboseStream << "Ignoring function \"" << fname << "\"" << std::endl;
    for (auto itt : data) {
      if(itt->get_label() == 0)
	itt->set_label(1); //label as normal event
    }
    return 0;
  }

  HbosParam& param = *(HbosParam*)m_param;
  HbosFuncParam &fparam = param[func_id];
  const Histogram &hist = fparam.getHistogram();

  unsigned long n_outliers = 0;

  auto const & bin_counts = hist.counts();
  auto const & bin_edges = hist.bin_edges();

  size_t nbin = bin_counts.size();
  size_t nedge = bin_edges.size();

  verboseStream << "Number of bins " << nbin << std::endl;

  //Check that the histogram contains bins
  if(nbin == 0){
    //As the pserver global model update is delayed, initially the clients may receive an empty model from the pserver for this function
    //Given that the model at this stage is unreliable anyway, we simply skip the function
    verboseStream << "Global model is empty, skipping outlier evaluation for function " << func_id << std::endl;
    return 0;
  }

  //For a histogram that has bins, the number of edges should be nbin+1
  if(nedge != nbin+1) fatal_error("Number of histogram edges is not 1 larger than the number of bins: #bins "+std::to_string(nbin)+" #edges "+std::to_string(nedge));

  //Bounds of the range of possible scores
  const double max_possible_score = -1 * log2(0.0 + m_alpha); //-log2(78.88e-32) ~ 100.0 by default (i.e. the theoretical max score)

  //Find the smallest and largest scores in the histogram (excluding empty bins)
  double min_score = std::numeric_limits<double>::max();
  double max_score = std::numeric_limits<double>::lowest();  

  //Compute scores
  double tot_runtimes = std::accumulate(bin_counts.begin(), bin_counts.end(), 0.0);
  std::vector<double> out_scores_i(nbin);

  verboseStream << "out_scores_i: " << std::endl;
  for(int i=0; i < nbin; i++){
    double count = bin_counts[i];
    double prob = count / tot_runtimes;
    double score = -1 * log2(prob + m_alpha);
    out_scores_i[i] = score;
    verboseStream << "Bin " << i << ", Range " << bin_edges[i] << "-" << bin_edges[i+1] << ", Count: " << count << ", Probability: " << prob << ", score: "<< score << std::endl;
    if(prob > 0 ) {
      min_score = std::min(min_score,score);
      max_score = std::max(max_score,score);
    }
  }
  verboseStream << std::endl;
  verboseStream << "min_score = " << min_score << std::endl;
  verboseStream << "max_score = " << max_score << std::endl;

  //Get the threshold appropriate to the function
  double hbos_threshold = getFunctionThreshold(fname);

#if 1
  //Compute threshold as a fraction of the range of scores in the histogram

  //Convert threshold into a score threshold
  double l_threshold = min_score + (hbos_threshold * (max_score - min_score));

  /*
    l_threshold = min_score*(1-m_threshold) + m_threshold*max_score
    min_score = -log2(p_max + m_alpha)
    max_score = -log2(p_min + m_alpha)
    l_threshold = -log2( [p_max + m_alpha]^[1-m_threshold] ) -log2( [p_min + m_alpha]^m_threshold )
    l_threshold = log2( [p_max + m_alpha]^[m_threshold-1] ) +log2( [p_min + m_alpha]^-m_threshold )
                = log2( [p_max + m_alpha]^-[1-m_threshold]  *  [p_min + m_alpha]^-m_threshold )
  */

  verboseStream << "Local threshold " << l_threshold << std::endl;
  if(m_use_global_threshold) {
    double g_threshold  = fparam.getInternalGlobalThreshold();
    verboseStream << "Global threshold before comparison with local threshold =  " << g_threshold << std::endl;

    if(l_threshold < g_threshold) {
      verboseStream << "Using global threshold as it is more stringent" << std::endl;
      l_threshold = g_threshold;
    } else {
      verboseStream << "Using local threshold as it is more stringent" << std::endl;
      fparam.setInternalGlobalThreshold(l_threshold);  //update the global score threshold to the new, more stringent test
    }
  }
#else
  //Rather than using the score range, which can become large for bins with small counts and is sensitive to the bin size and to bin shaving,
  //we can define the threshold based on the relative probability of a bin to the peak bin
  //p_i / p_max <= Q
  //Q = 1- hbos_threshold   as hbos_threshold default 0.99

  //s=-log2(p + alpha)

  //Threshold at 
  //p_i = Q p_max
  //2^-s_i -alpha = Q ( 2^-s_min - alpha )
  //s_i = -log2 [ Q 2^-s_min + (1-Q)alpha ]
  if(hbos_threshold >= 1.0) fatal_error("Invalid threshold value");
  double Q = 1-hbos_threshold;
  double l_threshold = -log2( Q*pow(2,-min_score) + (1.-Q)*m_alpha );
  
  verboseStream << "Condition p_i/p_max <= " << Q << " maps to local threshold " << l_threshold << std::endl;
#endif

  //Compute HBOS based score for each datapoint
  const double bin_width = hist.bin_edges().at(1) - hist.bin_edges().at(0);
  verboseStream << "Bin width: " << bin_width << std::endl;

  //Maintain the normal execution with the lowest score (most likely)
  bool lowest_score_set = false;
  double lowest_score_val = std::numeric_limits<double>::max();
  CallListIterator_t lowest_score_it;

  int top_out = 0;
  for (auto itt : data) {
    if (itt->get_label() == 0) {

      const double runtime_i = this->getStatisticValue(*itt); //runtimes.push_back(this->getStatisticValue(*itt));
      double ad_score;

      verboseStream << "Locating " << itt->get_json().dump() << std::endl;
      const int bin_ind = hist.getBin(runtime_i, 0.05); //allow events within 5% of the bin width away from the histogram edges to be included in the first/last bin
      verboseStream << "bin_ind: " << bin_ind << " for runtime_i: " << runtime_i << ", where bin_edges Size:" << nedge << " & num_bins: "<< nbin << std::endl;

      if( bin_ind == Histogram::LeftOfHistogram || bin_ind == Histogram::RightOfHistogram){
	//Sample (datapoint) lies outside of the histogram
	verboseStream << runtime_i << " is on " << (bin_ind == Histogram::LeftOfHistogram ? "left" : "right")  << " of histogram and an outlier" << std::endl;
	ad_score = max_possible_score;
	verboseStream << "ad_score(max_possible_score): " << ad_score << std::endl;
      }else{
	//Sample is within the histogram
	verboseStream << runtime_i << " maybe be an outlier" << std::endl;
	ad_score = out_scores_i[bin_ind];
	verboseStream << "ad_score(else): " << ad_score << ", bin_ind: " << bin_ind  << ", num_bins: " << nbin << ", out_scores_i size: " << out_scores_i.size() << std::endl;
      }

      //handle when ad_score = 0
      //This is valid when there is only one bin as the probability is 1 and log(1) = 0
      //Note that the total number of bins can be > 1 providing the other bins have 0 counts
      if (ad_score <= 0 ){
	int nbin_nonzero = 0;
	for(double c : hist.counts())
	  if(c>0) ++nbin_nonzero;
	if(nbin_nonzero != 1){
	  fatal_error("ad_score <= 0 but #bins with non zero count, "+std::to_string(nbin_nonzero)+" is not 1");
	}
      }


      itt->set_outlier_score(ad_score);
      verboseStream << "ad_score: " << ad_score << ", l_threshold: " << l_threshold << std::endl;

      //Compare the ad_score with the threshold
      if (ad_score >= l_threshold) {
	itt->set_label(-1);
	verboseStream << "!!!!!!!Detected outlier on func id " << func_id << " (" << itt->get_funcname() << ") on thread " << itt->get_tid() << " runtime " << runtime_i << " score " << ad_score << " (threshold " << l_threshold << ")" << std::endl;
	outliers.insert(itt, Anomalies::EventType::Outlier); //insert into data structure containing captured anomalies
	n_outliers += 1;
      }else {
        //Capture maximum of one normal execution per io step
        itt->set_label(1);

	//Record the normal event with the lowest score (most likely)
	if(ad_score < lowest_score_val){
	  lowest_score_val = ad_score;
	  lowest_score_it = itt;
	  lowest_score_set = true;
	}

      }

    }//if unlabeled point
  } //loop over data points

  //Record only the normal event with the lowest score
  if(lowest_score_set){
    auto itt = lowest_score_it;
    verboseStream << "Recorded normal event on func id " << func_id << " (" << itt->get_funcname() << ") on thread " << itt->get_tid() << " runtime " << this->getStatisticValue(*itt) << " score " << itt->get_outlier_score() << " (threshold " << l_threshold << ")" << std::endl;
    outliers.insert(itt, Anomalies::EventType::Normal);
  }

  return n_outliers;
}


/* ---------------------------------------------------------------------------
 * Implementation of ADOutlierCOPOD class
 * --------------------------------------------------------------------------- */
ADOutlierCOPOD::ADOutlierCOPOD(OutlierStatistic stat, double threshold, bool use_global_threshold) : ADOutlier(stat), m_alpha(78.88e-32), m_threshold(threshold), m_use_global_threshold(use_global_threshold) {
    m_param = new CopodParam();
}

ADOutlierCOPOD::~ADOutlierCOPOD() {
  if (m_param)
    m_param->clear();
}

double ADOutlierCOPOD::getFunctionThreshold(const std::string &fname) const{
  double copod_threshold = m_threshold; //default threshold
  //check to see if we have an override
  auto it = m_func_threshold_override.find(fname);
  if(it != m_func_threshold_override.end())
    copod_threshold = it->second;
  return copod_threshold;
}


Anomalies ADOutlierCOPOD::run(int step) {
  Anomalies outliers;
  if (m_execDataMap == nullptr) return outliers;

  //Generate the statistics based on this IO step
  CopodParam param;
  for (auto it : *m_execDataMap) { //loop over functions (key is function index)
    unsigned long func_id = it.first;
    CopodFuncParam &fparam = param[func_id];
    Histogram &hist = fparam.getHistogram();
    std::vector<double> runtimes;
    for (auto itt : it.second) { //loop over events for that function
      if (itt->get_label() == 0)
	runtimes.push_back(this->getStatisticValue(*itt));
    }
    verboseStream << "Function " << func_id << " has " << runtimes.size() << " unlabeled data points of " << it.second.size() << std::endl;

    if (runtimes.size() > 0) {
      verboseStream << "Creating local histogram for func " << func_id << " for " << runtimes.size() << " data points" << std::endl;
      hist.create_histogram(runtimes);
    }
    verboseStream << "Function " << func_id << " generated histogram has " << hist.counts().size() << " bins:" << std::endl;
    verboseStream << hist << std::endl;
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

//Compute the COPOD score
  //We take the score as the largest of:
  // 1) the avg of the right and left tailed 
  // 2) the skewness corrected ECDF value
inline double copod_score(const double runtime_i, const Histogram &hist, const Histogram &nhist, 
			  const double m_alpha, const int p_sign, const int n_sign,
			  Histogram::empiricalCDFworkspace &w, Histogram::empiricalCDFworkspace &nw){
  double left_tailed_prob = hist.empiricalCDF(runtime_i, &w);
  double right_tailed_prob = nhist.empiricalCDF(-runtime_i, &nw);

  //When generating the histogram we place the lower bound just before the minimum value
  //This means the CDF for the minimum value is exactly 0 whereas it should be at least 1/N
  //This introduces an error in COPOD; whenever a new minimum is encountered, it is marked as an outlier even if it should not be
  //To correct for this, if a data point is larger than min the CDF is shifted by 1/N
  verboseStream << "Runtime " << runtime_i << " hist min " << hist.getMin() << std::endl;
  if(runtime_i >= hist.getMin() ){ 
    verboseStream << "Shifting left-tailed prob from " << left_tailed_prob << " to ";
    left_tailed_prob += 1./w.getSum(hist); 
    left_tailed_prob = std::min(1.0, left_tailed_prob); 
    verboseStreamAdd << left_tailed_prob << std::endl;
  }
  verboseStream << "Negated runtime " << -runtime_i << " nhist min " << nhist.getMin() << std::endl;
  if(-runtime_i >= nhist.getMin() ){ 
    verboseStream << "Shifting right-tailed prob from " << right_tailed_prob << " to ";
    right_tailed_prob += 1./nw.getSum(nhist); 
    right_tailed_prob = std::min(1.0, right_tailed_prob); 
    verboseStreamAdd << right_tailed_prob << std::endl;
  }

  double left_tailed_score = -log2(left_tailed_prob + m_alpha);
  double right_tailed_score = -log2(right_tailed_prob + m_alpha);
  double avg_lr_score = (left_tailed_score + right_tailed_score)/2.;
  double corrected_score = (left_tailed_score * -1 * p_sign) + (right_tailed_score * n_sign);
  
  double ad_score = std::max(avg_lr_score, corrected_score);
  verboseStream << "Runtime: " << runtime_i 
		<< " left-tailed prob: " << left_tailed_prob << " left-tailed score: " << left_tailed_score 
		<< " right-tailed prob: " << right_tailed_prob << " right-tailed score: " << right_tailed_score 
		<< " avg l/r score: " << avg_lr_score << " skewness corrected score: " << corrected_score 
		<< " final score: " << ad_score << std::endl;
  return ad_score;
}

unsigned long ADOutlierCOPOD::compute_outliers(Anomalies &outliers,
					      const unsigned long func_id,
					      std::vector<CallListIterator_t>& data){

  verboseStream << "Finding outliers in events for func " << func_id << std::endl;
  verboseStream << "data Size: " << data.size() << std::endl;

  if(data.size() == 0) return 0;
  const std::string &fname = data.front()->get_funcname();

  //Check whether we are ignoring this function
  if(ignoringFunction(fname)){
    verboseStream << "Ignoring function \"" << fname << "\"" << std::endl;
    for (auto itt : data) {
      if(itt->get_label() == 0)
	itt->set_label(1); //label as normal event
    }
    return 0;
  }

  CopodParam& param = *(CopodParam*)m_param;
  CopodFuncParam &fparam = param[func_id];
  Histogram &hist = fparam.getHistogram();

  auto const & bin_counts = hist.counts();
  auto const & bin_edges = hist.bin_edges();

  size_t nbin = bin_counts.size();
  size_t nedge = bin_edges.size();

  verboseStream << "Number of bins " << nbin << std::endl;

  verboseStream << "Histogram:" << std::endl << hist << std::endl;

  //Check that the histogram contains bins
  if(nbin == 0){
    //As the pserver global model update is delayed, initially the clients may receive an empty model from the pserver for this function
    //Given that the model at this stage is unreliable anyway, we simply skip the function
    verboseStream << "Global model is empty, skipping outlier evaluation for function " << func_id << std::endl;
    return 0;
  }

  //For a histogram that has bins, the number of edges should be nbin+1
  if(nedge != nbin+1) fatal_error("Number of histogram edges is not 1 larger than the number of bins: #bins "+std::to_string(nbin)+" #edges "+std::to_string(nedge));

  //Compute the skewness of the histogram
  double skewness = hist.skewness();
  const int p_sign = (skewness - 1) < 0 ? -1 : (skewness - 1) > 0 ? 1 : 0; //sign(skewness-1)
  const int n_sign = (skewness + 1) < 0 ? -1 : (skewness + 1) > 0 ? 1 : 0; //sign(skewness+1)

  //Negate the histogram to compute right-tailed ECDFs
  Histogram nhist = -hist;
  verboseStream << "Inverted Histogram: " << std::endl << nhist << std::endl;

  //Determine the outlier threshold by computing the range of scores for data points *in the histogram*
  verboseStream << "Computing score range from histogram" << std::endl;
  Histogram::empiricalCDFworkspace w, nw;
  double min_score = -1 * log2(0.0 + m_alpha);   //Compute the range of scores in order to determine the outlier threshold
  double max_score = log2(1.0 + m_alpha) - min_score;
  
  for(int b=0;b<nbin;b++){
    double v = hist.binValue(b);
    double ad_score = copod_score(v, hist, nhist, m_alpha, p_sign, n_sign, w, nw);

    min_score = std::min(ad_score, min_score);
    max_score = std::max(ad_score, max_score);
  }
  verboseStream << "min_score = " << min_score << std::endl;
  verboseStream << "max_score = " << max_score << std::endl;

  //Compute threshold
  //Get the threshold appropriate to the function
  double func_threshold = getFunctionThreshold(fname); 

  double l_threshold = (max_score < 0) ? (-1 * func_threshold * (max_score - min_score)) : min_score + (func_threshold * (max_score - min_score));
  verboseStream << "l_threshold computed: " << l_threshold << std::endl;
  if(m_use_global_threshold) {
    double g_threshold = fparam.getInternalGlobalThreshold();
    verboseStream << "Global threshold before comparison with local threshold =  " << g_threshold << std::endl;
    if(l_threshold < g_threshold && g_threshold > (-1 * log2(1.00001))) {
      l_threshold = g_threshold;
    } else {
      fparam.setInternalGlobalThreshold(l_threshold);
    }
  }

  verboseStream << "Performing outlier detection" << std::endl;
  //Perform outlier detection  
  unsigned long n_outliers = 0;
  for (auto itt : data) {
    if (itt->get_label() == 0) {

      const double runtime_i = this->getStatisticValue(*itt); //runtimes.push_back(this->getStatisticValue(*itt));
      double ad_score = copod_score(runtime_i, hist, nhist, m_alpha, p_sign, n_sign, w, nw);
      
      itt->set_outlier_score(ad_score);
      verboseStream << "runtime: " << runtime_i << " ad_score: " << ad_score << ", l_threshold: " << l_threshold << std::endl;

      //Compare the ad_score with the threshold
      if (ad_score >= l_threshold) {
	itt->set_label(-1);
	verboseStream << "!!!!!!!Detected outlier on func id " << func_id << " (" << itt->get_funcname() << ") on thread " << itt->get_tid() << " runtime " << runtime_i << std::endl;
	outliers.insert(itt, Anomalies::EventType::Outlier); //insert into data structure containing captured anomalies
	++n_outliers;
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

std::vector<double> ADOutlierCOPOD::empiricalCDF(const std::vector<double>& runtimes, const bool sorted) {

  std::vector<double> tmp_runtimes = runtimes;
  auto ecdf = boost::math::empirical_cumulative_distribution_function(std::move(tmp_runtimes));
  std::vector<double> result_ecdf = std::vector<double>(runtimes.size(), 0.0);
  for(int i=0; i < runtimes.size(); i++) {
    result_ecdf.at(i) = ecdf(runtimes.at(i));
  }

  return result_ecdf;

}
