#include<chimbuko_config.h>
#include<chimbuko/util/Histogram.hpp>
#include<chimbuko/util/error.hpp>
#include<chimbuko/verbose.hpp>

using namespace chimbuko;

/**
 * @brief Histogram Class Implementation
 */


Histogram::Histogram(){clear();}
Histogram::~Histogram(){}

Histogram::Histogram(const std::vector<double> &data, const double bin_width): Histogram(){
  create_histogram(data, bin_width);
}


/**
 * @brief Merge Histogram
 */
Histogram chimbuko::operator+(const Histogram& g, const Histogram& l) {
  verboseStream << "Histogram merge: Counts Size of Global Histogram: " << g.counts().size() << ", Counts Size of Local Histogram: " << l.counts().size() << std::endl;

  if (g.counts().size() == 0) {
    verboseStream << "Global Histogram is empty, setting result equal to local histogram" << std::endl;
    if(l.bin_edges().size()!=l.Nbin()+1) fatal_error("Invalid local histogram");
    return l;
  }
  if (l.counts().size() == 0) {
    verboseStream << "Local Histogram is empty, setting result equal to global histogram" << std::endl;
    if(g.bin_edges().size()!=g.Nbin()+1) fatal_error("Invalid global histogram");
    return g;
  }

  //Both histograms are non-empty
  if(l.bin_edges().size()!=l.Nbin()+1) fatal_error("Invalid local histogram");
  if(g.bin_edges().size()!=g.Nbin()+1) fatal_error("Invalid global histogram");

  //The bin width is computed from the standard deviation of the combined histogram data, assuming that a bin of count n is represented by n copies of its midpoint value
  double bin_width = Histogram::scottBinWidth(g.counts(), g.bin_edges(), l.counts(), l.bin_edges());  /**< Compute bin width for merged histogram*/

  verboseStream << "BIN WIDTH while merging: " << bin_width << std::endl;

  /**
   * Compute most minimum bin edges and most maximum bin edges from two histograms (g & l)
   */
  double min_runtime = std::min( l.binValue(0),  g.binValue(0) );
  double max_runtime = std::max( l.binValue(l.Nbin()-1), g.binValue(g.Nbin()-1) );

  verboseStream << "Range of merged histogram " << min_runtime << "-" << max_runtime << std::endl;
      
  Histogram combined;
  std::vector<double> comb_binedges;
  std::vector<int> comb_counts;

  if ( bin_width / max_runtime < 1e-12){ //Can occur when the standard deviation of the histogram data set is 0 within rounding errors. This should only happen when both histograms have just 1 bin containing data and with the same lower bin edge
    verboseStream << "BINWIDTH is Zero within rounding errors" << std::endl;
    int l_nonempty = 0; 
    int l_b;
    for(int i=0;i<l.Nbin();i++) if(l.counts()[i] > 0){ ++l_nonempty; l_b = i; }

    int g_nonempty = 0; 
    int g_b;
    for(int i=0;i<g.Nbin();i++) if(g.counts()[i] > 0){ ++g_nonempty; g_b = i; }

    if(l_nonempty > 1 || g_nonempty > 1 || l.bin_edges()[l_b] != g.bin_edges()[g_b]) fatal_error("Encountered unexpected 0 bin width in histogram merge");

    //Just use the global histogram and update its bin count
    combined = g;
    combined.add2counts(g_b, l.counts()[l_b]);

    verboseStream << "Merged histogram has " << combined.counts().size() << " bins" << std::endl;
    return combined;
  }

  //bin_width is > 0
   
  //Compute the bin edges
  verboseStream << "BindWidth is > 0 here: " << std::endl;
   
  verboseStream << "min_runtime:" << min_runtime << std::endl;
  verboseStream << "max_runtime:" << max_runtime << std::endl;
  if (max_runtime < min_runtime) fatal_error("Incorrect boundary for runtime");

  double start = min_runtime - 0.01*bin_width; //start just below the first value because lower bounds are exclusive

  if (min_runtime == max_runtime) {
    comb_binedges.resize(2);

    comb_binedges[0] = start;
    comb_binedges[1] = max_runtime;
  }
  else{
    //Note the histogram bin upper edge is inclusive
	 
    //Generate edges < max_runtime. Last resulting upper edge is below max_runtime
    for(double edge_val = start; edge_val < max_runtime; edge_val += bin_width) {
      comb_binedges.push_back(edge_val);
    }
    //Add edge covering max_runtime
    comb_binedges.push_back( comb_binedges.back() + bin_width );
	
    if(comb_binedges.size() < 2) fatal_error("#Bin edges must be a minimum of 2!");
    if(max_runtime <= comb_binedges[comb_binedges.size()-2] || max_runtime > comb_binedges[comb_binedges.size()-1]) fatal_error("Logic bomb: max_runtime is not inside the last bin!");

    verboseStream << "Iterating between " << min_runtime << " and " << max_runtime << " in steps of " << bin_width << " resulted in " << comb_binedges.size() << " edges" << std::endl;
  }
   
  comb_counts = std::vector<int>(comb_binedges.size() - 1, 0);

  //Bin data points from global histogram
  for(int i=0; i<g.Nbin(); i++){
    double v = g.binValue(i);
    const int inc = g.counts().at(i);
    if(v <= start) fatal_error("Logic bomb: value below or equal to lower bin edge");
    int id( floor( (v-start) / bin_width ) ); //uniform bin width
    if(id < 0 || id >= comb_counts.size()) fatal_error("Logic bomb: bin beyond range of merged histogram");

    verboseStream << "In g " << "id: " << id << ", inc: " << inc << std::endl;
    comb_counts[id] += inc;
  }

  //Bin data points from local histogram
  for(int i=0; i<l.Nbin(); i++){
    double v = l.binValue(i);
    const int inc = l.counts().at(i);
    if(v <= start) fatal_error("Logic bomb: value below or equal to lower bin edge");
    int id( floor( (v-start) / bin_width ) ); //uniform bin width
    if(id < 0 || id >= comb_counts.size()) fatal_error("Logic bomb: bin beyond range of merged histogram");

    verboseStream << "In l " << "id: " << id << ", inc: " << inc << std::endl;
    comb_counts[id] += inc;
  }

  //Decide new threshold as greater of the two
  double new_threshold = std::max( l.get_threshold(),  g.get_threshold() );

  //Insert data into histogram
  combined.set_glob_threshold(new_threshold);
  combined.set_counts(comb_counts);
  combined.set_bin_edges(comb_binedges);
  verboseStream << "Merged histogram has " << combined.counts().size() << " bins" << std::endl;
  return combined;
}

Histogram& Histogram::operator+=(const Histogram& h)
{

  Histogram combined = *this + h;


  *this = combined;
  //this->set_hist_data(Histogram::Data(this->get_threshold(), this->counts(), this->bin_edges()));
  return *this;
}


double Histogram::binValue(const size_t i, const std::vector<double> & edges){
  return 0.5 * (edges[i+1] + edges[i]);
}
  


double Histogram::scottBinWidth(const std::vector<int> & global_counts, const std::vector<double> & global_edges, const std::vector<int> & local_counts, const std::vector<double> & local_edges){
  double sum = 0.0;
  verboseStream << "Size of Vector global_counts: " << global_counts.size() << std::endl;
  verboseStream << "Size of Vector local_counts: " << local_counts.size() << std::endl;

  verboseStream << "Global histogram counts: ";
  int size = 0;
  for(int i = 0; i < global_counts.size(); i++) {
    int count = global_counts[i];
    if (count < 0) fatal_error("Negative count encountered in global_counts!");
    //count = -1 * count;
    if (count != 0){
      verboseStreamAdd << global_edges[i]<<"-"<<global_edges[i+1] << ":" << count << std::endl;
    }
    size += count;
    sum += count * binValue(i,global_edges);
  }
  verboseStream << std::endl;
  verboseStream << "Size in scottBinWidth: " << size << std::endl;
  verboseStream << "Global sum in scottBinWidth: " << sum << std::endl;

  verboseStream << "Local histogram counts: ";
  for(int i = 0; i < local_counts.size(); i++) {
    int count = local_counts[i];
    if (count < 0) fatal_error("Negative count encountered in local_counts!");
    //count = -1 * count;
    if (count != 0){
      verboseStreamAdd << local_edges[i]<<"-"<<local_edges[i+1] << ":" << count << std::endl;
    }
    size += count;
    sum += count * binValue(i,local_edges);
  }
  verboseStream << std::endl;
  verboseStream << "Total size in scottBinWidth: " << size << std::endl;
  verboseStream << "Total sum in scottBinWidth: " << sum << std::endl;

  if(size == 0) fatal_error("Sum of bin counts over both histograms is zero!");

  const double mean = sum / size;
  verboseStream << "mean in _xcott_binWidth: " << mean << std::endl;

  double var = 0.0, std=0.0;
  for (int i=0;i<global_counts.size();i++){
    var += global_counts.at(i) * pow(binValue(i, global_edges) - mean, 2);
  }
  verboseStream << "Global var in scottBinWidth: " << var << std::endl;
  for (int i=0;i<local_counts.size();i++){
    var += local_counts.at(i) * pow(binValue(i, local_edges) - mean, 2);
  }
  verboseStream << "total var in scottBinWidth: " << var << std::endl;

  var = var / size;
  verboseStream << "Final Variance in scottBinWidth: " << var << std::endl;
  std = sqrt(var);
  verboseStream << "stddev in merging scottBinWidth: " << std << std::endl;
  //if (std <= 100.0) {return 0;}

  return ((3.5 * std ) / pow(size, 1./3));

}

double Histogram::scottBinWidth(const std::vector<double> & vals){
  //Find bin width as per Scott's rule = 3.5*std*n^-1/3

  double sum = std::accumulate(vals.begin(), vals.end(), 0.0);

  double mean = sum / vals.size();
  double var = 0.0, std = 0.0;
  for(int i=0; i<vals.size(); i++){
    var += pow(vals.at(i) - mean, 2);
  }
  var = var / vals.size();
  std = sqrt(var);
  verboseStream << "mean in scottBinWidth: " << mean << std::endl;
  verboseStream << "stddev in scottBinWidth: " << std << std::endl;

  return ((3.5 * std ) / pow(vals.size(), 1./3));
}

void Histogram::set_hist_data(const Histogram::Data& d)
{
  m_histogram.glob_threshold = d.glob_threshold;
  m_histogram.counts = d.counts;
  m_histogram.bin_edges = d.bin_edges;
}

//void Histogram::push (double x)
//{
// m_histogram.runtimes.push_back(x);
//}

void Histogram::create_histogram(const std::vector<double>& r_times, double bin_width){
  if(r_times.size() == 0) fatal_error("No data points provided");

  //If there is only one data point or all data points have the same value we cannot use the Scott bin width rule because the std.dev is 0
  //Instead just put the bin edges +-1% around the point   
  bool all_same = true;
  for(size_t i=1;i<r_times.size();i++) if(r_times[i] != r_times[0]){ all_same =false; break; }
   
  if(all_same){
    double delta = 0.01 * fabs(r_times[0]);
    if(delta == 0.) delta = 0.001; //if the value is 0 we cannot infer a scale

    m_histogram.bin_edges.resize(2);
    m_histogram.bin_edges[0] = r_times[0] - delta;
    m_histogram.bin_edges[1] = r_times[0] + delta;
    m_histogram.counts.resize(1);
    m_histogram.counts[0] = r_times.size();
    verboseStream << "Histogram::create_histogram all data points have same value. Creating 1-bin histogram with edges " << m_histogram.bin_edges[0] << "," << m_histogram.bin_edges[1] << std::endl;
    return;
  }

  //Determine the bin width
  std::vector<double> runtimes = r_times;       
  if(bin_width == 0.){
    bin_width = Histogram::scottBinWidth(runtimes);
    if (bin_width <= 0){
      fatal_error("Scott binwidth returned an invalid value "+std::to_string(bin_width));
    }
  }else{
    if(bin_width < 0.) fatal_error("Invalid bin width");
  }

  std::sort(runtimes.begin(), runtimes.end());
  const int h = runtimes.size() - 1;

  if (m_histogram.bin_edges.size() > 0) m_histogram.bin_edges.clear();

  double first_edge = runtimes[0] - 0.01 * bin_width; //lower edges are exclusive and we want the first data point inside the first bin

  m_histogram.bin_edges.push_back(first_edge);

  double prev = m_histogram.bin_edges.at(0);
  while(prev < runtimes.at(h)){ //no action if prev >= last data point, ensuring last data point is always included (upper bounds are inclusive)
    m_histogram.bin_edges.push_back(prev + bin_width);
    prev += bin_width;
  }
  verboseStream << "Number of bins: " << m_histogram.bin_edges.size()-1 << std::endl;

  if (m_histogram.counts.size() > 0) m_histogram.counts.clear();
  m_histogram.counts = std::vector<int>(m_histogram.bin_edges.size()-1, 0);
  for ( int i=0; i < runtimes.size(); i++) {
    for ( int j=1; j < m_histogram.bin_edges.size(); j++) {
      if ( runtimes.at(i) <= m_histogram.bin_edges.at(j) ) { //upper edge inclusive
	m_histogram.counts[j-1] += 1;
	break;
      }
    }
  }
  verboseStream << "Size of counts: " << m_histogram.counts.size() << std::endl;

  //Check sum of counts is equal to the number of data points
  int count_sum = 0;
  for(int c: m_histogram.counts) count_sum += c;
  if(count_sum != runtimes.size()) fatal_error("Histogram bin total count does not match number of data points");

  //m_histogram.runtimes.clear();
  const double min_threshold = -1 * log2(1.00001);
  if (!(m_histogram.glob_threshold > min_threshold)) {
    m_histogram.glob_threshold = min_threshold;
  }
  this->set_hist_data(Histogram::Data( m_histogram.glob_threshold, m_histogram.counts, m_histogram.bin_edges ));
}

std::vector<double> Histogram::unpack() const{
  const int tot_size = std::accumulate(counts().begin(), counts().end(), 0);   
  std::vector<double> r_times(tot_size);

  int idx=0;
  for (int i = 0; i < Nbin(); i++) {
    double v = binValue(i,bin_edges());
    for(int j = 0; j < counts()[i]; j++){ 
      r_times.at(idx++) = v;
    }
  }
  return r_times;
}

int Histogram::totalCount() const{
  int c = 0; 
  for(auto v: counts()) c += v;
  return c;
}


void Histogram::merge_histograms(const Histogram& g, const std::vector<double>& runtimes)
{
  const int tot_size = runtimes.size() + std::accumulate(g.counts().begin(), g.counts().end(), 0);   
  std::vector<double> r_times(tot_size); // = runtimes;
  int idx = 0;
  verboseStream << "Number of runtime events during mergin: " << runtimes.size() << std::endl;
  verboseStream << "total number of 'g' bin_edges: " << g.bin_edges().size() << std::endl;

  //Fix for XGC run where unlabelled func_id is retained causing Zero bin_edges
  if (g.bin_edges().size() == 0){
    this->create_histogram(runtimes);
    return;
  }

  //Unwrapping the histogram
  for (int i = 0; i < g.Nbin(); i++) {
    verboseStream << " Bin counts in " << i << ": " << g.counts()[i] << std::endl;
    double v = binValue(i,g.bin_edges());
    for(int j = 0; j < g.counts()[i]; j++){ 
      r_times.at(idx++) = v;
    }
  }

  for (int i = 0; i< runtimes.size(); i++)
    r_times.at(idx++) = runtimes.at(i);

  m_histogram.glob_threshold = g.get_threshold();
  //verboseStream << "glob_threshold in merge_histograms = " << m_histogram.glob_threshold << std::endl;
  this->create_histogram(r_times);
  //this->set_hist_data(Histogram::Data( m_histogram.glob_threshold, m_histogram.counts, m_histogram.bin_edges ));
}

nlohmann::json Histogram::get_json() const {
  return {
    {"Histogram Bin Counts", m_histogram.counts},
      {"Histogram Bin Edges", m_histogram.bin_edges}};
}

int Histogram::getBin(const double v, const double tol) const{
  if(Nbin() == 0) fatal_error("Histogram is empty");
  if(bin_edges().size() < 2) fatal_error("Histogram has <2 bin edges");

  size_t nbin = Nbin();
  double first_bin_width = bin_edges()[1] - bin_edges()[0];
  double last_bin_width = bin_edges()[nbin] - bin_edges()[nbin-1];
   
  if(v <= bin_edges()[0] - tol*first_bin_width) return LeftOfHistogram; //lower edges are exclusive bounds
  else if(v <= bin_edges()[0]) return 0; //within tolerance of first bin
  else if(v > bin_edges()[nbin] + tol*last_bin_width) return RightOfHistogram;
  else if(v > bin_edges()[nbin]) return nbin-1; //within tolerance of last bin
  else{
    for(int j=1; j <= nbin; j++){       
      if(v <= bin_edges()[j]){
	return (j-1);
      }
    }
    fatal_error("Logic bomb: should not reach here!");
  }
}


int Histogram::empiricalCDFworkspace::getSum(const Histogram &h){
  if(!set){
    verboseStream << "Workspace computing sum" << std::endl;
    sum = h.totalCount();
    set = true;
  }
  return sum;
}


double Histogram::empiricalCDF(const double value, empiricalCDFworkspace *workspace) const{
  int bin = getBin(value,0.);
  if(bin == LeftOfHistogram) return 0.;
  else if(bin == RightOfHistogram) return 1.;

  //CDF is defined from count ( values <= value )
  //We always include the full count of the bin as we do not know the distribution of the data within the bin, and assuming the bin is represented by the midpoint can lead to significant errors in outlier detection in practise  
  int count = 0;
  for(int i=0;i<=bin;i++) count += m_histogram.counts[i];

  int sum;
  if(workspace != nullptr) sum = workspace->getSum(*this);
  else sum = totalCount();
  
  return double(count)/sum;
}

Histogram Histogram::operator-() const{
  Histogram out(*this);
  Histogram::Data &d = out.m_histogram;
  for(int b=0; b< d.bin_edges.size(); b++) d.bin_edges[b] = -d.bin_edges[b];
  std::reverse(d.bin_edges.begin(),d.bin_edges.end());
  std::reverse(d.counts.begin(),d.counts.end());
  return out;
}

double Histogram::skewness() const{
  //<(a-b)^3> = <a^3 - b^3 - 3a^2 b + 3b^2 a>
  //<(x-mu)^3> = <x^3> - mu^3 - 3<x^2> mu + 3 mu^2 <x>
  //           = <x^3> - 3<x^2> mu + 2 mu^3
  double avg_x3 = 0, avg_x2 = 0, avg_x = 0;
  int csum = 0;
  for(int b=0;b<Nbin();b++){
    double v = binValue(b);
    int c = m_histogram.counts[b];
    avg_x3 += c*pow(v,3);
    avg_x2 += c*pow(v,2);
    avg_x += c*v;
    csum += c;
  }
  avg_x3 /= csum;
  avg_x2 /= csum;
  avg_x /= csum;

  double var = avg_x2 - avg_x*avg_x;  //<x^2> - mu^2
  
  double avg_xmmu_3 = avg_x3 - 3*avg_x2 * avg_x + 2 * pow(avg_x,3);
  return double(csum)/(csum-1) * avg_xmmu_3 / pow(var, 3./2);
}





namespace chimbuko{
  std::ostream & operator<<(std::ostream &os, const Histogram &h){
    for(int i=0;i<h.counts().size();i++){
      os << h.bin_edges()[i] << "-" << h.bin_edges()[i+1] << ":" << h.counts()[i];
      if(i<h.counts().size()-1) os << std::endl;
    }
    return os;
  }
}
