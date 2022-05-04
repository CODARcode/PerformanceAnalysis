#include<chimbuko_config.h>
#include<chimbuko/util/Histogram.hpp>
#include<chimbuko/util/error.hpp>
#include<chimbuko/verbose.hpp>
#include <sstream>
#include <limits>

using namespace chimbuko;

double binWidthScott::bin_width(const std::vector<double> &runtimes, const double min, const double max) const{ return Histogram::scottBinWidth(runtimes); }

double binWidthScott::bin_width(const Histogram &a, const Histogram &b, const double min, const double max) const{ return Histogram::scottBinWidth(a.counts(),a.bin_edges(),b.counts(),b.bin_edges()); }

double binWidthFixedNbin::bin_width(const std::vector<double> &runtimes, const double min, const double max) const{ 
  if(max == min) fatal_error("Maximum and minimum value are the same!");
  
  double x = Histogram::getLowerBoundShiftMul();
  
  //The first bin edge is placed  x*bin_width before min,  and we need 
  //bin(max) = int( floor( (max-first_edge)/bin_width ) ) == nbin-1
  
  //floor( (max-first_edge)/bin_width ) + 1 = nbin
  //floor( (max-min+x*bin_width)/bin_width ) + 1 = nbin
  //floor( (max-min)/bin_width + x ) + 1 = nbin
  
  //nbin-1 <= (max-min)/bin_width + x < nbin
  //nbin-1-x <= (max-min)/bin_width < nbin-x

  //a solution is
  //bin_width = (max-min)/(nbin - x - 0.001)

  return (max - min)/(nbin-x-0.001);
}

double binWidthFixedNbin::bin_width(const Histogram &a, const Histogram &b, const double min, const double max) const{
  double x = Histogram::getLowerBoundShiftMul();
  return (max - min)/(nbin-x-0.001);
}

double binWidthMaxNbinBase::correct_bin_width(double bin_width, const double min, const double max) const{
  double x = Histogram::getLowerBoundShiftMul();

  //The first bin edge is placed  x*bin_width before min,  and we need  nbin <= maxbins
  //range_start = min - x*bin_width  
  //nbin = ceil( (max - range_start)/bin_width ) = ceil( (max-min)/bin_width + x )
  if(bin_width == 0. || int(ceil( (max-min)/bin_width + x)) > maxbins)
    return (max - min)/(maxbins-x-0.001); //use same solution as the fixedNbin functions
  else
    return bin_width;
}
  

void Histogram::Data::clear() {
  glob_threshold = log2(1.00001);
  counts.clear();
  bin_edges.clear();
  min = std::numeric_limits<double>::max();
  max = std::numeric_limits<double>::lowest();
}


void Histogram::set_min_max(double min, double max){ 
  if(min <= binEdges(0).first || max > binEdges(Nbin()-1).second) fatal_error("Values must be within the histogram!");
  m_histogram.min = min; 
  m_histogram.max = max; 
}



/**
 * @brief Histogram Class Implementation
 */


Histogram::Histogram(){clear();}
Histogram::~Histogram(){}

Histogram::Histogram(const std::vector<double> &data, const binWidthSpecifier &bwspec): Histogram(){
  create_histogram(data, bwspec);
}

double Histogram::getLowerBoundShiftMul(){ return 1e-6; }

double Histogram::uniformCountInRange(double l, double u) const{
  if(u<=l) fatal_error(std::string("Invalid range, require u>l but got l=") + std::to_string(l) + " u=" + std::to_string(u));

  if(m_histogram.max == m_histogram.min){
    //Ignore the bin edges, the data set is a delta function
    double v = m_histogram.max;
    int dbin = getBin(v,0);
    double count = binCount(dbin);
    verboseStream << "uniformCountInRange range " << l << ":" << u << " evaluating for max=min=" << v << ": data are in bin " << dbin << " with count " << count << " : l<v?" << int(l<v) << " u>=v?" << int(u>=v);
    if(l < v && u>= v){
      verboseStreamAdd << " returning " << count << std::endl;
      return count;
    }else{
      verboseStreamAdd << " returning " << 0 << std::endl;
      return 0;
    }
  }

  int lbin = getBin(l,0);
  if(lbin == Histogram::LeftOfHistogram){
    lbin=0;
    l=binEdges(0).first;
  }else if(lbin == Histogram::RightOfHistogram){//ubin must also be right of histogram
    return 0;
  }

  int ubin = getBin(u,0);
  if(ubin == Histogram::RightOfHistogram){
    ubin=Nbin()-1;
    u=binEdges(ubin).second;
  }else if(ubin == Histogram::LeftOfHistogram){//lbin must also be left of histogram
    return 0; 
  }
  
  auto lbinedges = binEdges(lbin);
  auto ubinedges = binEdges(ubin);
  double lbincount = binCount(lbin);
  double ubincount = binCount(ubin);

  if(lbin == ubin){
    //If in the same bin 
    double count = (u - l) / (lbinedges.second - lbinedges.first) * lbincount;
    assert(count <= lbincount);
    return count;
  }else{
    double lbinfrac = (lbinedges.second - l)/(lbinedges.second - lbinedges.first) * lbincount;
    double ubinfrac = (u - ubinedges.first)/(ubinedges.second - ubinedges.first) * ubincount;
    
    assert(lbinfrac <= lbincount);
    assert(ubinfrac <= ubincount);
    
    double count = lbinfrac + ubinfrac;
    for(int b=lbin+1;b<=ubin-1;b++) count += binCount(b);

    return count;
  }
}

void Histogram::merge_histograms_uniform(Histogram &combined, const Histogram& g, const Histogram& l){
  std::vector<double> &comb_binedges = combined.m_histogram.bin_edges;
  std::vector<double> &comb_counts = combined.m_histogram.counts;
  
  int nbin_merged = comb_counts.size();
  double new_total = 0;

  for(int b=0;b<nbin_merged;b++){
    double gc = g.uniformCountInRange(comb_binedges[b], comb_binedges[b+1]);
    double lc = l.uniformCountInRange(comb_binedges[b], comb_binedges[b+1]);
    double val = gc+lc;
    verboseStream << "Bin " << b << " range " << comb_binedges[b] << " to " << comb_binedges[b+1] << ": gc=" << gc << " lc=" << lc << " val=" << val << std::endl;
    
    comb_counts[b] += val;
    new_total += val;
  }

  //Due to rounding issues the total number of points in the merged histogram can differ from the sum of the points in the
  //inputs. This is undesirable. To fix this while minimizing the impact we apply the changes over the largest bin
  double ltotal = l.totalCount();
  double gtotal = g.totalCount();
  if( fabs(new_total - ltotal - gtotal) > 1e-5*(ltotal+gtotal) ){
    std::stringstream ss;
    ss << "New histogram total count doesn't match sum of counts of inputs: combined total " << new_total << " l total " << ltotal << " g total " << gtotal << " l+g total " << ltotal + gtotal << " diff " << fabs(new_total - ltotal - gtotal);    
    recoverable_error(ss.str());
  }
}



void Histogram::merge_histograms_central_value(Histogram &combined, const Histogram& g, const Histogram& l){
  std::vector<double> &comb_binedges = combined.m_histogram.bin_edges;
  std::vector<double> &comb_counts = combined.m_histogram.counts;
  double start = comb_binedges[0];
  double bin_width = comb_binedges[1]-comb_binedges[0]; //assume uniform bin width

  //Bin data points from global histogram
  for(int i=0; i<g.Nbin(); i++){
    double v = g.binValue(i);
    const double inc = g.counts().at(i);
    if(v <= start) fatal_error("Logic bomb: value below or equal to lower bin edge");
    int id( floor( (v-start) / bin_width ) ); //uniform bin width
    if(id < 0 || id >= comb_counts.size()) fatal_error("Logic bomb: bin beyond range of merged histogram");

    verboseStream << "In g " << "id: " << id << ", inc: " << inc << std::endl;
    comb_counts[id] += inc;
  }

  //Bin data points from local histogram
  for(int i=0; i<l.Nbin(); i++){
    double v = l.binValue(i);
    const double inc = l.counts().at(i);
    if(v <= start) fatal_error("Logic bomb: value below or equal to lower bin edge");
    int id( floor( (v-start) / bin_width ) ); //uniform bin width
    if(id < 0 || id >= comb_counts.size()) fatal_error("Logic bomb: bin beyond range of merged histogram");

    verboseStream << "In l " << "id: " << id << ", inc: " << inc << std::endl;
    comb_counts[id] += inc;
  }
}

/**
 * @brief Merge Histogram
 */
Histogram Histogram::merge_histograms(const Histogram& g, const Histogram& l, const binWidthSpecifier &bwspec) {
  verboseStream << "Histogram::merge_histograms merging local histogram:\n" << l << "\nand global histogram:\n" << g << std::endl;

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

  if(l.m_histogram.min == std::numeric_limits<double>::max() || l.m_histogram.max == std::numeric_limits<double>::lowest()) fatal_error("Local histogram min/max values are uninitialized!");
  if(g.m_histogram.min == std::numeric_limits<double>::max() || g.m_histogram.max == std::numeric_limits<double>::lowest()) fatal_error("Global histogram min/max values are uninitialized!");

  double min = std::min(l.m_histogram.min,g.m_histogram.min);
  double max = std::max(l.m_histogram.max,g.m_histogram.max);
  if(max < min) fatal_error("Min and max values out of order");

  verboseStream << "Histogram::merge_histograms local hist min, max = " << l.m_histogram.min << ", " << l.m_histogram.max << "  global histogram min, max = " << g.m_histogram.min << ", " << g.m_histogram.max << std::endl;

  double bin_width = bwspec.bin_width(g,l,min,max);
  if(bin_width < 0.) fatal_error("Suggested bin width " + std::to_string(bin_width) + " is invalid");

  if(bin_width == 0. || ceil((max-min)/bin_width) > 50000. ){ //Can occur when the standard deviation of the histogram data set is 0 within rounding errors. This should only happen when both histograms have just 1 bin containing data and with the same lower bin edge
    verboseStream << "Suggested bin width " << bin_width << " is too small, resulting in too many bins!" << std::endl;

    //Choose the smaller of the bin widths of the two inputs
    bin_width = std::min(g.bin_edges()[1]-g.bin_edges()[0], l.bin_edges()[1]-l.bin_edges()[0]);
    if(bin_width == 0. || ceil((max-min)/bin_width) > 50000. ) fatal_error("Failed to determine the appropriate bin width");
  }

  verboseStream << "Histogram::merge_histograms suggested bin width " << bin_width << std::endl;

  double range_start = min - getLowerBoundShiftMul() * bin_width; //lower edges are exclusive and we want the first data point inside the first bin
  double range_end = max;
  verboseStream << "Histogram::merge_histograms determined range " << range_start << ":" << range_end << std::endl;

  if(range_start == range_end){ 
    //This can happen if min and max are the same and the suggested bin width is very small: the result is that the shift of range_start below min vanishes by a floating point error
    //If this happens we adjust such that there is 1 bin
    if(min == max){
      range_start = min - bin_width;
      if(range_start == range_end){
	//If the bin width was too small that the range still evaluates to 0 we must modify the bin width too
	if(min == 0.) bin_width = 1e-6;
	else bin_width = 1e-6*min;
	range_start = min - bin_width;
	if(range_start == range_end) fatal_error("Unable to correct for 0 range!");
      }
    }else{
      std::stringstream ss; ss << "Logic bomb: range_start and range_end are equal despite shift:" << range_start << ". min=" << min << " max=" << max << " bin width=" << bin_width <<  " vshift=" << getLowerBoundShiftMul() * bin_width << std::endl;
      fatal_error(ss.str());
    }
  }

  //Adjust the bin width such that the upper edge is equal to the max value
  int nbin = int( ceil( (range_end - range_start)/bin_width ) );
  if(nbin < 0) fatal_error("Negative bin count encountered, possible integer overflow");
  if(nbin == 0) fatal_error("Number of bins is 0!");
  verboseStream << "Histogram::merge_histograms number of bins " << nbin << std::endl; 

  bin_width = (range_end - range_start)/nbin;
  verboseStream << "Histogram::merge_histograms adjusted bin width " << bin_width << std::endl;
  verboseStream << "Histogram::merge_histograms min=" << min << " max=" << max << " chosen range (" << range_start << ")-(" << range_end << "), bin width=" << bin_width << " and number of bins " << nbin << std::endl;
     
  //Compute the bin edges
  Histogram combined;
  combined.m_histogram.min = min;
  combined.m_histogram.max = max;
  std::vector<double> &comb_binedges = combined.m_histogram.bin_edges;
  std::vector<double> &comb_counts = combined.m_histogram.counts;

  comb_binedges.resize(nbin+1);
  for(int e=0;e<=nbin;e++) comb_binedges[e] = range_start + e*bin_width;
	
  if(comb_binedges.size() < 2) fatal_error("#Bin edges must be a minimum of 2!");
    
  if(fabs(comb_binedges[nbin]-range_end) > 1e-12*bin_width){
    std::stringstream ss; ss << "Logic bomb: upper bin edge " << comb_binedges[nbin] << "of merged histogram does not match expected range_end=" << range_end;
    fatal_error(ss.str());
  }
  comb_binedges[nbin] = range_end; //correct for floating point errors

  verboseStream << "Iterating between " << range_start << " and " << range_end << " in steps of " << bin_width << " resulted in " << comb_binedges.size() << " edges" << std::endl;
   
  comb_counts.resize(comb_binedges.size() - 1, 0);

  //merge_histograms_central_value(combined, g, l);
  merge_histograms_uniform(combined, g, l);

  //Decide new threshold as greater of the two
  combined.m_histogram.glob_threshold = std::max( l.get_threshold(),  g.get_threshold() );

  verboseStream << "Merged histogram has " << combined.counts().size() << " bins" << std::endl;

  verboseStream << "Histogram::merge_histograms merged local histogram:\n" << l << "\nand global histogram:\n" << g << "\nobtained:\n" << combined << std::endl;

  return combined;
}



double Histogram::binValue(const size_t i, const std::vector<double> & edges){
  return 0.5 * (edges[i+1] + edges[i]);
}
  


double Histogram::scottBinWidth(const std::vector<double> & global_counts, const std::vector<double> & global_edges, const std::vector<double> & local_counts, const std::vector<double> & local_edges){
  verboseStream << "scottBinWidth (2 histograms)" << std::endl << "Size of Vector global_counts: " << global_counts.size() << std::endl << "Size of Vector local_counts: " << local_counts.size() << std::endl;

  double size = 0., avgx = 0., avgx2 = 0.;

  for(int i = 0; i < global_counts.size(); i++) {
    double count = global_counts[i];
    if (count < 0) fatal_error("Negative count encountered in global_counts!");
    size += count;
    double v = binValue(i,global_edges);
    avgx += count * v;
    avgx2 += count * v*v;
  }
  for(int i = 0; i < local_counts.size(); i++) {
    double count = local_counts[i];
    if (count < 0) fatal_error("Negative count encountered in local_counts!");
    size += count;
    double v = binValue(i,local_edges);
    avgx += count * v;
    avgx2 += count * v*v;
  }

  verboseStream << "Size : " << size << " sum(x)=" << avgx << " and sum(x^2)=" << avgx2 << std::endl;

  if(size == 0) fatal_error("Sum of bin counts over both histograms is zero!");

  avgx /= size;
  avgx2 /= size;

  verboseStream << "Mean in scottBinWidth: " << avgx << std::endl;

  double var = avgx2 - avgx*avgx;
  if(var < 0) var = 0; //can happen due to floating point errors

  double std = sqrt(var);

  verboseStream << "Final Variance in scottBinWidth: " << var << std::endl << "stddev in merging scottBinWidth: " << std << std::endl;

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


double Histogram::scottBinWidth(const std::vector<double> & global_counts, const std::vector<double> & global_edges, const std::vector<double> & local_vals){
  double sum = 0.0;
  double sum_sq = 0.0;
  double size = 0;
  for(int i = 0; i < global_counts.size(); i++) {
    double count = global_counts[i];
    if (count < 0) fatal_error("Negative count encountered in global_counts!");
    if (count != 0){
      verboseStreamAdd << global_edges[i]<<"-"<<global_edges[i+1] << ":" << count << std::endl;
    }
    size += count;
    double bv = binValue(i,global_edges);
    sum += count * bv;
    sum_sq += count * bv*bv;
  }
  for(double v: local_vals){
    sum += v;
    sum_sq += v*v;
    size += 1.0;
  }
  if(size == 0) fatal_error("Sum of bin counts over both histograms is zero!");

  double mean = sum / size;
  double var = sum_sq/size - pow(mean,2);
  double std = sqrt(var);
  return ((3.5 * std ) / pow(size, 1./3));
}


void Histogram::set_hist_data(const Histogram::Data& d)
{
  m_histogram = d;
}

void Histogram::create_histogram(const std::vector<double>& r_times, const binWidthSpecifier &bwspec){
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
    m_histogram.min = m_histogram.max = r_times[0];
    verboseStream << "Histogram::create_histogram all data points have same value. Creating 1-bin histogram with edges " << m_histogram.bin_edges[0] << "," << m_histogram.bin_edges[1] << std::endl;
    return;
  }

  //Determine the range
  double &min = m_histogram.min;
  double &max = m_histogram.max;
  
  min = std::numeric_limits<double>::max();
  max = std::numeric_limits<double>::lowest();
  for(double v: r_times){
    min = std::min(min,v);
    max = std::max(max,v);
  }
  verboseStream << "Histogram::create_histogram min=" << min << " max=" << max << std::endl;

  //Determine the bin width
  double bin_width = bwspec.bin_width(r_times, min, max);
  if(bin_width < 0.) fatal_error("Invalid bin width");

  double first_edge = min - getLowerBoundShiftMul() * bin_width; //lower edges are exclusive and we want the first data point inside the first bin
  
  //Adjust the bin width such that the upper edge is equal to the max value
  int nbin = int( ceil( (max - first_edge)/bin_width ) );
  bin_width = (max - first_edge)/nbin;

  verboseStream << "Histogram::create_histogram determined bin width " << bin_width << " and number of bins " << nbin << std::endl;
  
  m_histogram.bin_edges.resize(nbin+1);
  double e = first_edge;
  for(int b=0;b<=nbin;b++){
    m_histogram.bin_edges[b] = e;
    e += bin_width;
  }

  double vshift = 1e-8*bin_width; //because lower bound is exclusive, a value lying exactly on a bin edge should be counted as part of the previous bin
  
  m_histogram.counts = std::vector<double>(nbin,0.);
  for(double v: r_times){
    int b = int(floor((v-vshift-first_edge)/bin_width) );
    if(b<0 || b>=nbin) fatal_error("Data point falls outside histogram range!");
    m_histogram.counts[b] += 1.0;
  }
  
  //Check sum of counts is equal to the number of data points
  double count_sum = 0;
  for(double c: m_histogram.counts) count_sum += c;
  if( fabs(count_sum - double(r_times.size())) > 1e-5) fatal_error("Histogram bin total count does not match number of data points");

  //m_histogram.runtimes.clear();
  const double min_threshold = -1 * log2(1.00001);
  if (!(m_histogram.glob_threshold > min_threshold)) {
    m_histogram.glob_threshold = min_threshold;
  }
}

std::vector<double> Histogram::unpack() const{
  int tot_size = 0;
  for(double c : counts()) tot_size += int(floor(c+0.5)); //have to round to nearest int to unpack

  std::vector<double> r_times(tot_size);

  int idx=0;
  for (int i = 0; i < Nbin(); i++) {
    double v = binValue(i,bin_edges());
    int icount = int(floor(counts()[i]+0.5));
    for(int j = 0; j < icount; j++){ 
      r_times.at(idx++) = v;
    }
  }
  return r_times;
}

double Histogram::totalCount() const{
  double c = 0; 
  for(auto v: counts()) c += v;
  return c;
}


Histogram Histogram::merge_histograms(const Histogram& g, const std::vector<double>& runtimes, const binWidthSpecifier &bwspec)
{
  int tot_size = runtimes.size();
  for(double c : g.counts()) tot_size += int(floor(c+0.5)); //have to round to nearest int to unpack

  std::vector<double> r_times(tot_size); // = runtimes;
  int idx = 0;
  verboseStream << "Number of runtime events during mergin: " << runtimes.size() << std::endl;
  verboseStream << "total number of 'g' bin_edges: " << g.bin_edges().size() << std::endl;

  //Fix for XGC run where unlabelled func_id is retained causing Zero bin_edges
  if (g.bin_edges().size() == 0){
    return Histogram(runtimes,bwspec);
  }

  //Unwrapping the histogram
  for (int i = 0; i < g.Nbin(); i++) {
    verboseStream << " Bin counts in " << i << ": " << g.counts()[i] << std::endl;
    double v = binValue(i,g.bin_edges());
    int icount = int(floor(g.counts()[i]+0.5)); //have to round to nearest int in order to unpack    
    for(int j = 0; j < icount; j++){ 
      r_times.at(idx++) = v;
    }
  }

  for (int i = 0; i< runtimes.size(); i++)
    r_times.at(idx++) = runtimes.at(i);

  Histogram out(r_times,bwspec);
  out.m_histogram.glob_threshold = g.get_threshold();
  return out;
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


double Histogram::empiricalCDFworkspace::getSum(const Histogram &h){
  if(!set){
    verboseStream << "Workspace computing sum" << std::endl;
    sum = h.totalCount();
    set = true;
  }
  return sum;
}


double Histogram::empiricalCDF(const double value, empiricalCDFworkspace *workspace) const{
  double sum;
  if(workspace != nullptr) sum = workspace->getSum(*this);
  else sum = totalCount();

  return uniformCountInRange(std::numeric_limits<double>::lowest(),value)/sum;
}

Histogram Histogram::operator-() const{
  Histogram out(*this);
  Histogram::Data &d = out.m_histogram;
  for(int b=0; b< d.bin_edges.size(); b++) d.bin_edges[b] = -d.bin_edges[b];
  std::reverse(d.bin_edges.begin(),d.bin_edges.end());
  std::reverse(d.counts.begin(),d.counts.end());
  d.max = -this->m_histogram.min;
  d.min = -this->m_histogram.max;
  return out;
}

double Histogram::skewness() const{
  //<(a-b)^3> = <a^3 - b^3 - 3a^2 b + 3b^2 a>
  //<(x-mu)^3> = <x^3> - mu^3 - 3<x^2> mu + 3 mu^2 <x>
  //           = <x^3> - 3<x^2> mu + 2 mu^3
  double avg_x3 = 0, avg_x2 = 0, avg_x = 0;
  double csum = 0;
  for(int b=0;b<Nbin();b++){
    double v = binValue(b);
    double c = m_histogram.counts[b];
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
  return csum/(csum-1.) * avg_xmmu_3 / pow(var, 3./2);
}

void Histogram::shiftBinEdges(const double x){
  for(auto &e : m_histogram.bin_edges)
    e += x;
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
