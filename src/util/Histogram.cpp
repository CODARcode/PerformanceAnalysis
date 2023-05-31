#include<chimbuko_config.h>
#include<chimbuko/util/Histogram.hpp>
#include<chimbuko/util/error.hpp>
#include<chimbuko/verbose.hpp>
#include <sstream>
#include <limits>

using namespace chimbuko;

double binWidthScott::bin_width(const std::vector<double> &runtimes, const double min, const double max) const{ return Histogram::scottBinWidth(runtimes); }

double binWidthScott::bin_width(const Histogram &a, const Histogram &b, const double min, const double max) const{ return Histogram::scottBinWidth(a,b); }

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
 

/**
 * @brief Histogram Class Implementation
 */


Histogram::Histogram(){clear();}
Histogram::~Histogram(){}

void Histogram::clear() {
  m_counts.clear(); 
  m_start = 0;
  m_bin_width = 0;
  m_min = std::numeric_limits<double>::max();
  m_max = std::numeric_limits<double>::lowest();
}

Histogram::Histogram(const std::vector<double> &data, const binWidthSpecifier &bwspec): Histogram(){
  create_histogram(data, bwspec);
}

std::pair<double,double> Histogram::binEdges(const int bin) const{
  double l = m_start + bin*m_bin_width;
  double u = (bin == Nbin()-1 ? m_max : l + m_bin_width);

  return {l,u};
}
double Histogram::binEdgeLower(const int bin) const{
  return m_start + bin*m_bin_width;
}
double Histogram::binEdgeUpper(const int bin) const{
  return (bin == Nbin()-1 ? m_max : m_start + (bin+1)*m_bin_width); //avoid rounding errors for last bin edge
}

double Histogram::binWidth() const{
  return m_bin_width;
}

double Histogram::getLowerBoundShiftMul(){ return 1e-6; }

double Histogram::uniformCountInRange(double l, double u) const{
  if(u<=l) fatal_error(std::string("Invalid range, require u>l but got l=") + std::to_string(l) + " u=" + std::to_string(u));

  if(m_max == m_min){
    //Ignore the bin edges, the data set is a delta function
    if(Nbin() != 1) fatal_error("Logic bomb: max=min but #bins != 1?");
    double v = m_max;
    int dbin = 0;
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
    l=binEdgeLower(0);
  }else if(lbin == Histogram::RightOfHistogram){//ubin must also be right of histogram
    return 0;
  }

  int ubin = getBin(u,0);
  if(ubin == Histogram::RightOfHistogram){
    ubin=Nbin()-1;
    u=m_max;
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

    assert(u>ubinedges.first && u<=ubinedges.second);
    
    assert(lbinfrac <= lbincount);
    assert(ubinfrac <= ubincount);
    
    double count = lbinfrac + ubinfrac;
    for(int b=lbin+1;b<=ubin-1;b++) count += binCount(b);

    return count;
  }
}

void Histogram::merge_histograms_uniform_int(Histogram &combined, const Histogram& g, const Histogram& l){
  std::vector<unsigned int> &comb_counts = combined.m_counts;

  //Use variable bin width histograms and use the extractUniformCountInRangeInt function to pull out data in ranges so as to ensure correct treatment of integer bins
  HistogramVBW gw(g), lw(l);
  
  int nbin_merged = comb_counts.size();
  double new_total = 0;

  std::vector<std::pair<double,double> > edges(nbin_merged);
  for(int b=0;b<nbin_merged;b++){
    edges[b] = combined.binEdges(b);
    if(b>0) edges[b].first = edges[b-1].second; //eliminate floating point errors
  }
  
  std::vector<double> gc = gw.extractUniformCountInRangesInt(edges);
  std::vector<double> lc = lw.extractUniformCountInRangesInt(edges);
  
  for(int b=0;b<nbin_merged;b++){
    unsigned int gcc = gc[b], lcc = lc[b];
    unsigned int val = lcc + gcc;
    verboseStream << "Bin " << b << " range " << edges[b].first << " to " << edges[b].second << ": gc=" << gcc << " lc=" << lcc << " val=" << val << std::endl;    
    comb_counts[b] += val;
    new_total += val;
  }

  if(lw.totalCount() != 0.){
    std::stringstream ss; ss << "Histogram l still contains data unmerged into the output: " << lw << std::endl;
    recoverable_error(ss.str());
  }
  if(gw.totalCount() != 0.){
    std::stringstream ss; ss << "Histogram g still contains data unmerged into the output: " << gw << std::endl;
    recoverable_error(ss.str());
  }

  unsigned int ltotal = l.totalCount();
  unsigned int gtotal = g.totalCount();
  if( new_total - ltotal - gtotal > 0 ){
    std::stringstream ss;
    ss << "New histogram total count doesn't match sum of counts of inputs: combined total " << new_total << " l total " << ltotal << " g total " << gtotal << " l+g total " << ltotal + gtotal << " diff " << fabs(new_total - ltotal - gtotal);    
    recoverable_error(ss.str());
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
    return l;
  }
  if (l.counts().size() == 0) {
    verboseStream << "Local Histogram is empty, setting result equal to global histogram" << std::endl;
    return g;
  }

  //Both histograms are non-empty
  if(l.m_min == std::numeric_limits<double>::max() || l.m_max == std::numeric_limits<double>::lowest()) fatal_error("Local histogram min/max values are uninitialized!");
  if(g.m_min == std::numeric_limits<double>::max() || g.m_max == std::numeric_limits<double>::lowest()) fatal_error("Global histogram min/max values are uninitialized!");

  double min = std::min(l.m_min,g.m_min);
  double max = std::max(l.m_max,g.m_max);
  if(max < min) fatal_error("Min and max values out of order");

  verboseStream << "Histogram::merge_histograms local hist min, max = " << l.m_min << ", " << l.m_max << "  global histogram min, max = " << g.m_min << ", " << g.m_max << std::endl;

  double bin_width = bwspec.bin_width(g,l,min,max);
  if(bin_width < 0.) fatal_error("Suggested bin width " + std::to_string(bin_width) + " is invalid");

  if(bin_width == 0. || ceil((max-min)/bin_width) > 50000. ){ //Can occur when the standard deviation of the histogram data set is 0 within rounding errors. This should only happen when both histograms have just 1 bin containing data and with the same lower bin edge
    verboseStream << "Suggested bin width " << bin_width << " is too small, resulting in too many bins!" << std::endl;

    //Choose the smaller of the bin widths of the two inputs
    bin_width = std::min(g.binWidth(), l.binWidth());
    if(bin_width == 0. || ceil((max-min)/bin_width) > 50000. ) fatal_error("Failed to determine the appropriate bin width");
  }

  verboseStream << "Histogram::merge_histograms suggested bin width " << bin_width << " cf local hist bin width " << l.binWidth() << " global hist bin width " << g.binWidth() << std::endl;

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
  combined.m_min = min;
  combined.m_max = max;
  combined.m_start = range_start;
  combined.m_bin_width = bin_width;
  combined.m_counts.resize(nbin, 0);

  merge_histograms_uniform_int(combined, g, l);

  verboseStream << "Merged histogram has " << combined.Nbin() << " bins" << std::endl;

  verboseStream << "Histogram::merge_histograms merged local histogram:\n" << l << "\nand global histogram:\n" << g << "\nobtained:\n" << combined << std::endl;

  return combined;
}
 
double Histogram::scottBinWidth(const Histogram &global, const Histogram &local){
  verboseStream << "scottBinWidth (2 histograms)" << std::endl << " with #Nbins " << local.Nbin() << " and " << global.Nbin() << std::endl;

  unsigned int size = 0;
  double avgx = 0., avgx2 = 0.;

  for(int i = 0; i < global.Nbin(); i++) {
    unsigned int count = global.binCount(i);
    size += count;
    double v = global.binValue(i);
    avgx += count * v;
    avgx2 += count * v*v;
  }
  for(int i = 0; i < local.Nbin(); i++) {
    unsigned int count = local.binCount(i);
    size += count;
    double v = local.binValue(i);
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


double Histogram::scottBinWidth(const Histogram & global, const std::vector<double> & local_vals){
  double sum = 0.0;
  double sum_sq = 0.0;
  unsigned int size = 0;
  for(int i = 0; i < global.Nbin(); i++) {
    unsigned int count = global.binCount(i);
    if (enableVerboseLogging() && count != 0){
      auto be = global.binEdges(i);
      verboseStreamAdd << be.first <<"-"<< be.second << ":" << count << std::endl;
    }
    size += count;
    double bv = global.binValue(i);
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


void Histogram::create_histogram(const std::vector<double>& data, const double min, const double max, const double start, const int nbin, const double bin_width){
  if(data.size() == 0) fatal_error("No data points provided");
  if(nbin < 1) fatal_error("Require at least 1 bin");
  if(min < start || min > start + bin_width) fatal_error("min point should lie within the first bin");
  if( fabs(start + nbin*bin_width - max) > 1e-8*bin_width ) fatal_error("Mismatch between {start,nbin,bin width} and max provided");
  
  m_min = min;
  m_max = max;
  m_counts = std::vector<unsigned int>(nbin,0);
  m_start = start;
  m_bin_width = bin_width;

  for(double d: data){
    int b = this->getBin(d);
    if(b == LeftOfHistogram || b == RightOfHistogram) fatal_error("One or more data points do not lie within the bins provided");
    ++m_counts[b];
  }
}

void Histogram::create_histogram(const std::vector<double>& r_times, const binWidthSpecifier &bwspec){
  if(r_times.size() == 0) fatal_error("No data points provided");

  //If there is only one data point or all data points have the same value we cannot use the Scott bin width rule because the std.dev is 0
  //Instead just put the bin edges +-x% around the point   
  bool all_same = true;
  for(size_t i=1;i<r_times.size();i++) if(r_times[i] != r_times[0]){ all_same =false; break; }
   
  if(all_same){
    double delta = 1e-12 * fabs(r_times[0]);
    if(delta == 0.) delta = 1e-12; //if the value is 0 we cannot infer a scale    
    
    m_start = r_times[0] - delta;
    m_bin_width = delta;
    m_min = m_max = r_times[0];

    m_counts.resize(1);
    m_counts[0] = r_times.size();
    verboseStream << "Histogram::create_histogram all data points have same value. Creating 1-bin histogram with edges " << m_start << "," << m_max << " bin width " << m_bin_width << " and min value above start by " << m_min-m_start << std::endl;
    return;
  }

  //Determine the range
  double min = std::numeric_limits<double>::max();
  double max = std::numeric_limits<double>::lowest();
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

  m_min = min;
  m_max = max;
  m_bin_width = bin_width;
  m_start = first_edge;
  
  if(fabs(m_start + nbin*m_bin_width - max) > 1e-8*bin_width) fatal_error("Unexpectedly large error in bin upper edge");

  m_counts = std::vector<unsigned int>(nbin,0);
  for(double v: r_times){
    int b = getBin(v,0.);
    if(b==LeftOfHistogram || b==RightOfHistogram){
      std::ostringstream os; os << "Value " << v << " is outside of histogram with range " << first_edge << " " << max << " diffs " << v-first_edge << " " << v-max;
      fatal_error(os.str());
    }
    m_counts[b] += 1.0;
  }
  
  //Check sum of counts is equal to the number of data points
  unsigned int count_sum = 0;
  for(unsigned int c: m_counts) count_sum += c;
  if( count_sum - r_times.size() != 0){
    std::ostringstream os; os << "Histogram bin total count " << count_sum << " does not match number of data points " << r_times.size();
    fatal_error(os.str());
  }
}


void Histogram::set_histogram(const std::vector<unsigned int> &counts, const double min, const double max, const double start, const double bin_width){
  if(max < min) fatal_error("Invalid min/max: max " + std::to_string(max) + " < min " + std::to_string(min));
  if(min < start || min > start + bin_width) fatal_error("Min point " + std::to_string(min) + " should lie within the first bin " + std::to_string(start) +"-"+std::to_string(start+bin_width)  );
  int nbin = counts.size();
  double max_expect = start + nbin*bin_width;
  if( fabs(max_expect- max) > 1e-8*bin_width ) fatal_error("Mismatch between {start,nbin,bin width} and max provided: max from start,bin_width: " + std::to_string(max_expect) + " is too far from provided max " + std::to_string(max) );

  m_counts = counts;
  m_min = min;
  m_max = max;
  m_start = start;
  m_bin_width = bin_width;
}
  



std::vector<double> Histogram::unpack() const{
  unsigned int tot_size = 0;
  for(unsigned int c : counts()) tot_size += c;

  std::vector<double> r_times(tot_size);

  int idx=0;
  for (int i = 0; i < Nbin(); i++) {
    double v = binValue(i);
    unsigned int icount = binCount(i);
    for(unsigned int j = 0; j < icount; j++){ 
      r_times[idx++] = v;
    }
  }
  return r_times;
}

unsigned int Histogram::totalCount() const{
  unsigned int c = 0; 
  for(auto v: counts()) c += v;
  return c;
}


Histogram Histogram::merge_histograms(const Histogram& g, const std::vector<double>& runtimes, const binWidthSpecifier &bwspec)
{
  unsigned int tot_size = runtimes.size();
  for(unsigned int c : g.counts()) tot_size += c;

  std::vector<double> r_times(tot_size); // = runtimes;
  int idx = 0;
  verboseStream << "Number of runtime events during mergin: " << runtimes.size() << std::endl;

  //Unwrapping the histogram
  for (int i = 0; i < g.Nbin(); i++) {
    verboseStream << " Bin counts in " << i << ": " << g.binCount(i) << std::endl;
    double v = g.binValue(i);
    unsigned int icount = g.binCount(i);
    for(unsigned int j = 0; j < icount; j++){ 
      r_times[idx++] = v;
    }
  }

  for (int i = 0; i< runtimes.size(); i++)
    r_times[idx++] = runtimes[i];

  Histogram out(r_times,bwspec);
  return out;
}

nlohmann::json Histogram::get_json() const {
  return {
    {"Bin Counts", m_counts},
      {"First edge", m_start},
	{"Bin width", m_bin_width},
	  {"Min", m_min},
	    {"Max", m_max}
  };
}


template<typename FT> 
std::enable_if_t<std::is_floating_point_v<FT>, FT> 
ulp(FT x)
{
  if (x > 0)
    return std::nexttoward(x, std::numeric_limits<FT>::infinity()) - x;
  else 
    return x - std::nexttoward(x, -std::numeric_limits<FT>::infinity());
}

int Histogram::getBin(const double v, const double tol) const{
  if(Nbin() == 0) fatal_error("Histogram is empty");

  size_t nbin = Nbin();

  if(v <= m_start - tol*m_bin_width) return LeftOfHistogram; //lower edges are exclusive bounds
  else if(v <= m_start) return 0; //within tolerance of first bin
  else if(v > m_max + tol*m_bin_width) return RightOfHistogram;
  else if(v >= m_max) return nbin-1; //within tolerance of last bin (upper edges are inclusive)
  else{
    //To avoid ambiguities associated with floating point errors, we *define* the bin edges as  m_start + b*m_bin_width,   apart from the final edge which is m_max
    double bb = (v-m_start)/m_bin_width;
    int bin = int(floor(bb));

    //It's possible that this value is still associated with the previous bin because of the ambiguity with floating point arithmetic
    //However as we define the bin edges as above, we can check
    double bu_prev = m_start + bin*m_bin_width;
    if(v <= bu_prev){
      --bin;
      verboseStream << "Value lies below or equal to lower bin edge " << bu_prev << " adjusting bin index to" << bin << std::endl;
    }

    // double diff = bb - bin;
    // verboseStream << "Value " << v << " calc bin (double) " << bb << " provisional bin " << bin << " with relative distance from lower edge " << diff << std::endl;
    // //Note, lower edge is exclusive, upper edge is inclusive; i.e. if the value falls on the lower edge of a bin it should be counted as in the previous bin
    // //However this can be ambiguous due to potential rounding errors, so we have a fixed tolerance for plausible errors
    // if(diff < 1e-10){
    //   --bin;
    //   verboseStream << "Value is within plausible rounding error of lower bin edge, assigning to previous bin," << bin << std::endl;
    // }


    if(bin<0) fatal_error("Logic bomb: bin outside of histogram");
    return bin;
  }
}

unsigned int Histogram::empiricalCDFworkspace::getSum(const Histogram &h){
  if(!set){
    verboseStream << "Workspace computing sum" << std::endl;
    sum = h.totalCount();
    set = true;
  }
  return sum;
}


double Histogram::empiricalCDF(const double value, empiricalCDFworkspace *workspace) const{
  unsigned int sum;
  if(workspace != nullptr) sum = workspace->getSum(*this);
  else sum = totalCount();

  return uniformCountInRange(std::numeric_limits<double>::lowest(),value)/sum;
}

Histogram Histogram::operator-() const{
  Histogram out(*this);
  std::reverse(out.m_counts.begin(),out.m_counts.end());
  out.m_max = -m_min;
  out.m_min = -m_max;
  out.m_start = out.m_min-getLowerBoundShiftMul()*m_bin_width;
  return out;
}

double Histogram::skewness() const{
  //<(a-b)^3> = <a^3 - b^3 - 3a^2 b + 3b^2 a>
  //<(x-mu)^3> = <x^3> - mu^3 - 3<x^2> mu + 3 mu^2 <x>
  //           = <x^3> - 3<x^2> mu + 2 mu^3
  double avg_x3 = 0, avg_x2 = 0, avg_x = 0;
  unsigned int csum = 0;
  for(int b=0;b<Nbin();b++){
    double v = binValue(b);
    unsigned int c = m_counts[b];
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
  return double(csum)/(csum-1.) * avg_xmmu_3 / pow(var, 3./2);
}

namespace chimbuko{
  std::ostream & operator<<(std::ostream &os, const Histogram &h){
    for(int i=0;i<h.Nbin();i++){
      auto be = h.binEdges(i);
      os << be.first << "-" << be.second << ":" << h.binCount(i);
      if(i<h.Nbin()-1) os << std::endl;
    }
    return os;
  }
}

std::string HistogramVBW::Bin::getBinInfo(Bin* bin){
  std::ostringstream os;
  if(!bin) os << "(null)";
  else os << *bin;
  return os.str();
}

HistogramVBW::Bin* HistogramVBW::Bin::getChainStart(Bin* begin){
  if(!begin) fatal_error("Provided bin is null");
  Bin* f= begin;
  while(f->left != nullptr) f = f->left;
  return f;
}

std::string HistogramVBW::Bin::getChainInfo(Bin* any_bin){
  std::ostringstream os;
  Bin* f = getChainStart(any_bin);
  while(!f->is_end){
    os << getBinInfo(f) << " ";
    f = f->right;
  }
  return os.str();
}


HistogramVBW::Bin* HistogramVBW::Bin::insertRight(HistogramVBW::Bin* of, HistogramVBW::Bin* toins){
  if(of == nullptr) fatal_error("Nullptr exception");
  if(of->is_end) fatal_error("Cannot insert right of end");
  if(toins->l < of->u || toins->u < of->u){
    std::ostringstream os; os << "Cannot insert " << getBinInfo(toins) << " right of " << getBinInfo(of) << " because the edges are not properly ordered: " 
			      << toins->l << "<" << of->u << "?" << (toins->l < of->u) << "  "
			      << toins->u << "<" << of->u << "?" << (toins->u < of->u) << std::endl;
    fatal_error(os.str());
  }

  toins->right = of->right;
  toins->right->left = toins;

  toins->left = of;
  of->right = toins;
  return toins;
}

HistogramVBW::Bin* HistogramVBW::Bin::insertLeft(HistogramVBW::Bin* of, HistogramVBW::Bin* toins){
  if(of == nullptr) fatal_error("Nullptr exception");
  if(!of->is_end && (toins->u > of->l || toins->l > of->l) ) fatal_error("Cannot insert " + getBinInfo(toins) + " left of " + getBinInfo(of) + " because the edges are not properly ordered");
 
  toins->left = of->left;
  if(toins->left) toins->left->right = toins;

  toins->right = of;
  of->left = toins;
  return toins;
}

HistogramVBW::Bin* create_end(){
  HistogramVBW::Bin* end = new HistogramVBW::Bin(0,0,0);  
  end->is_end = true;
  return end;
}


std::pair<HistogramVBW::Bin*,HistogramVBW::Bin*> HistogramVBW::Bin::insertFirst(HistogramVBW::Bin* toins){
  if(toins == nullptr) fatal_error("Nullptr exception");
  Bin* end = create_end();
  insertLeft(end, toins);
  return {toins,end};
}
  


void HistogramVBW::Bin::deleteChain(HistogramVBW::Bin* first){
  if(first == nullptr) fatal_error("Nullptr exception");
  if(first->left != nullptr) fatal_error("Deleting must start at the beginning of the chain");

  Bin* cur = first;
  Bin* next = cur->right;
  while(next != nullptr){
    delete cur;
    cur = next;
    next = cur->right;
  }
  if(!cur->is_end) fatal_error("Chain is broken");
  delete cur;
}

HistogramVBW::Bin* HistogramVBW::Bin::getBin(HistogramVBW::Bin* start, double v){
  if(start == nullptr) fatal_error("Nullptr exception");
  if(start->is_end || v <= start->l) return nullptr;
  
  Bin* cur = start;
  Bin* next = cur->right;
  while(!cur->is_end){
    if(v <= cur->u){
      if(v <= cur->l){
	std::ostringstream os;
	os << "Logic error in bin search, expect " << v << " to be within bin " << getBinInfo(cur) << " but the value is below the lower edge. Possibly the chain edges are not ordered? Chain: " << getChainInfo(start);
	fatal_error(os.str());
      }
      return cur;
    }
    cur = next;
    next = cur->right;
  }
  return nullptr;
}

    
std::pair<HistogramVBW::Bin*,HistogramVBW::Bin*> HistogramVBW::Bin::split(HistogramVBW::Bin* bin, double about){
  if(bin == nullptr || bin->is_end) fatal_error("Invalid bin");
  if(about <= bin->l || about > bin->u){
    std::ostringstream os;
    os << "Split location " << about << " not inside bin " << getBinInfo(bin) << "\n"
       << "Tests for exclusion  about < lower_edge: " << (about < bin->l) << "  about > upper edge: " << (about > bin->u) << "\n"
       << "Left neighbor bin " << getBinInfo(bin->left) << " and right neighbor bin " << getBinInfo(bin->right);
    fatal_error(os.str());
  }

  verboseStream << "Splitting bin " << *bin << " about " << about << std::endl;
  if(about == bin->u){
    verboseStream << "Split point matches upper edge, doing nothing" << std::endl;
    return {bin,bin->right}; //nothing needed
  }
  
  double bw = bin->u - bin->l;
  double o[2];
  double fracs[2] = { (about - bin->l)/bw, 0 };
  fracs[1] = 1. - fracs[0];
  double count = bin->c;
  
  int lrg = 0;
  double debt = count;
  for(int i=0;i<2;i++){
    o[i] = floor(fracs[i] * count + 0.5);
    debt -= o[i];
    if(fracs[i] > fracs[lrg]) lrg = i;
  }
  
  //Assign debt to largest fraction with preference from left
  o[lrg] += debt;
  double bu_prev = bin->u;
  bin->u = about;
  bin->c = o[0];
  
  insertRight(bin, new Bin(about, bu_prev, o[1]));
  
  verboseStream << "Split bin into " << *bin << " and " << *bin->right << std::endl;
  return {bin, bin->right};
}

size_t HistogramVBW::Bin::size(HistogramVBW::Bin* first){
  if(first == nullptr) return 0;
  
  Bin* cur = first;
  size_t sz = 0;
  while(!cur->is_end){
    ++sz;
    cur = cur->right;
  }
  return sz;
}

std::ostream & chimbuko::operator<<(std::ostream &os, const HistogramVBW::Bin &b){
  if(b.is_end) os << "(END)";
  else os << "(" << b.l << "," << b.u << "]{" << b.c << "}";
  return os;
}

std::ostream & chimbuko::operator<<(std::ostream &os, const HistogramVBW &h){
  HistogramVBW::Bin const* cur = h.getFirst();
  if(cur == nullptr) return os;

  while(!cur->is_end){
    os << *cur << " ";
    cur = cur->right;
  }
  return os;
}





void HistogramVBW::set_min_max(double min, double max){ 
  m_min = min; 
  m_max = max; 
}

size_t HistogramVBW::Nbin() const{
  return Bin::size(first);
}

HistogramVBW::Bin const* HistogramVBW::getBin(const double v) const{ return Bin::getBin(first, v); }

HistogramVBW::Bin const* HistogramVBW::getBinByIndex(const int idx) const{
  if(first == nullptr) return nullptr;

  int curidx = 0; 
  Bin* cur = first;
  
  while(!cur->is_end){
    if(idx == curidx) return cur;
    cur = cur->right;
    ++curidx;
  }
  return nullptr;
}

void HistogramVBW::import(const Histogram &h){
  if(h.Nbin() == 0) return;

  auto be = h.binEdges(0);
  auto fe = Bin::insertFirst(new Bin(be.first, be.second, h.binCount(0)) );
  first = fe.first;
  end = fe.second;

  //for this histogram we want to ensure the upper edge of the previous bin and the lower edge of the next
  //are identical. As the basic histogram computes these edges up to potential floating point errors, we enforce it manually here
  double prev_upper = be.second; 

  Bin* hp = first;
  for(int b=1;b<h.Nbin();b++){
    be = h.binEdges(b);
    double reldiff_l = 2*(be.first - prev_upper)/(be.first + prev_upper);
    if( fabs(reldiff_l) > 1e-8  ){
      std::ostringstream of; of << "Lower bin edge should match upper bin edge of previous bin! Got " << prev_upper << " "  << be.first << " reldiff " << reldiff_l;
      fatal_error(of.str());
    }

    hp = Bin::insertRight(hp, new Bin(prev_upper, be.second, h.binCount(b)) );
    prev_upper = be.second;
  }
  m_min = h.getMin();
  m_max = h.getMax();
}

HistogramVBW::~HistogramVBW(){
  if(first != nullptr) Bin::deleteChain(first);
}

double HistogramVBW::totalCount() const{
  if(first == nullptr) return 0;
  
  Bin* cur = first;
  double out = 0;
  while(!cur->is_end){
    out += cur->c;
    cur = cur->right;
  }
  return out;
}


double HistogramVBW::extractUniformCountInRangeInt(double l, double u){
  verboseStream << "Extracting count in range " << l << ":" << u << std::endl;
  if(u<=l) fatal_error(std::string("Invalid range, require u>l but got l=") + std::to_string(l) + " u=" + std::to_string(u));
  if(first == nullptr) fatal_error("Histogram is empty");

  if(m_max == m_min){
    //Ignore the bin edges, the data set is a delta function
    double v = m_max;
    Bin* bin = (Bin*)getBin(v);
    double count = bin->c;
    verboseStream << "extractUniformCountInRangeInt range " << l << ":" << u << " evaluating for max=min=" << v << ": data are in bin with count " << count << " : l<v?" << int(l<v) << " u>=v?" << int(u>=v);
    if(l < v && u>= v){
      bin->c = 0;
      verboseStreamAdd << " returning " << count << std::endl;
      return count;
    }else{
      verboseStreamAdd << " returning " << 0 << std::endl;
      return 0;
    }
  }

  Bin* last = end->left;
  Bin* bl = Bin::getBin(first, l);

  if(bl != nullptr){
    verboseStream << "Lower edge " << l << " in bin " << *bl << std::endl;
    bl = Bin::split(bl,l).second;
    if(bl->is_end){
      verboseStream << "Right of split point is end" << std::endl;
      //If the split point matches the upper edge of the last bin, the .second pointer is END
      return 0;
    }
  }else if(l <= first->l){ //left edge is left of histogram
    bl = first;
    verboseStream << "Lower edge is left of histogram" << std::endl;
  }else if(l > last->u){ //left edge is right of histogram
    return 0;
  }else{
    assert(0);
  }

  last = end->left; //update last in case it changed

  Bin* bu = Bin::getBin(bl, u);
  if(bu != nullptr){
    bu = Bin::split(bu,u).first;
  }else if(u <= first->l){ //right edge is left of histogram
    return 0;
  }else if(u > last->u){ //right edge is right of histogram
    verboseStream << "Upper edge is right of histogram" << std::endl;
    bu = last;
  }else{
    assert(0);
  }

  verboseStream << "Zeroing bins between " << *bl << " and " << *bu << std::endl;
  Bin* h = bl;
  double out = 0;
  while(h != bu){
    out += h->c;
    h->c = 0;
    h = h->right;
  }
  
  out += h->c;
  h->c = 0;
  return out;
}


std::vector<double> HistogramVBW::extractUniformCountInRangesInt(const std::vector<std::pair<double,double> > &edges){
  std::vector<double> out(edges.size(), 0.);
  if(edges.size()==0) return out;

  if(first == nullptr) fatal_error("Histogram is empty");

  if(m_max == m_min){
    //Ignore the bin edges, the data set is a delta function
    double v = m_max;
    Bin* bin = (Bin*)getBin(v);
    double count = bin->c;
    for(size_t idx = 0; idx < edges.size(); idx++){
      auto const &be = edges[idx];
      double l = be.first, u = be.second;
      if(u<=l) fatal_error(std::string("Invalid range, require u>l but got l=") + std::to_string(l) + " u=" + std::to_string(u));

      if(l < v && u>= v){
	verboseStream << "extractUniformCountInRangeInt range " << l << ":" << u << " evaluating for max=min=" << v << ": data are in bin with count " << count << std::endl;
	bin->c = 0;
	out[idx] = count;
	break; //all future entries already 0
      }
    }
  }

  Bin* last = end->left;
  Bin* prev_upper = first;

  for(size_t idx = 0; idx < edges.size(); idx++){
    auto const &be = edges[idx];
    double l = be.first, u = be.second;
    if(u<=l) fatal_error(std::string("Invalid range, require u>l but got l=") + std::to_string(l) + " u=" + std::to_string(u));
    if(idx>0 && l< edges[idx-1].second) fatal_error("Expect edges to be ordered");

    Bin* bl = Bin::getBin(prev_upper, l);

    if(bl != nullptr){
      verboseStream << "Lower edge " << l << " in bin " << *bl << std::endl;
      bl = Bin::split(bl,l).second;
      if(bl->is_end){
	verboseStream << "Right of split point is end" << std::endl;
	//If the split point matches the upper edge of the last bin, the .second pointer is END and the entry is 0
	//furthermore, all entries with edges >=l will also have 0 entries, so we don't need to continue
	return out;
      }
    }else if(l <= first->l){ //left edge is left of histogram
      bl = first;
      verboseStream << "Lower edge is left of histogram" << std::endl;
    }else if(l > last->u){ //left edge is right of histogram
      return out;
    }else{
      assert(0);
    }

    last = end->left; //update last in case it changed
    
    Bin* bu = Bin::getBin(bl, u);
    if(bu != nullptr){
      bu = Bin::split(bu,u).first;
    }else if(u <= first->l){ //right edge is left of histogram
      continue;
    }else if(u > last->u){ //right edge is right of histogram
      verboseStream << "Upper edge is right of histogram" << std::endl;
      bu = last;
    }else{
      assert(0);
    }

    last = end->left;
    
    verboseStream << "Zeroing bins between " << *bl << " and " << *bu << std::endl;
    Bin* h = bl;
    while(h != bu){
      out[idx] += h->c;
      h->c = 0;
      h = h->right;
    }
  
    out[idx] += h->c;
    h->c = 0;
    
    prev_upper = bu;
  }
  
  return out;
}
