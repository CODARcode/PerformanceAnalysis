#include "chimbuko/param/hbos_param.hpp"
#include "chimbuko/verbose.hpp"
#include "chimbuko/util/error.hpp"
#include <sstream>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/access.hpp>
#include <cereal/types/vector.hpp>
#include <limits>
#include <cmath>

#define MIN(x, y) x < y ? x : y;
#define MAX(x, y) x > y ? x : y;

using namespace chimbuko;

/**
 * HBOS based implementation
 */
 HbosParam::HbosParam() {
   clear();
 }

 HbosParam::~HbosParam(){

 }

 void HbosParam::clear()
 {
     m_hbosstats.clear();
 }

 void HbosParam::show(std::ostream& os) const
 {
     os << "\n"
        << "Parameters: " << m_hbosstats.size() << std::endl;

     for (auto stat: m_hbosstats)
     {
         os << "Function " << stat.first << std::endl;
         os << stat.second.get_json().dump(2) << std::endl;
     }

     os << std::endl;
 }

 void HbosParam::assign(const std::unordered_map<unsigned long, Histogram>& hbosstats)
 {
     std::lock_guard<std::mutex> _(m_mutex);
     for (auto& pair: hbosstats) {
         m_hbosstats[pair.first] = pair.second;
     }
 }

 void HbosParam::assign(const std::string& parameters)
 {
     std::unordered_map<unsigned long, Histogram> hbosstats;

     deserialize_cerealpb(parameters, hbosstats);
     assign(hbosstats);
 }

 std::string HbosParam::serialize() const
 {
     std::lock_guard<std::mutex> _{m_mutex};

     return serialize_cerealpb(m_hbosstats);
 }

 std::string HbosParam::serialize_cerealpb(const std::unordered_map<unsigned long, Histogram>& hbosstats)
 {
   std::unordered_map<unsigned long, Histogram::Data> histdata;
   for(auto const& e : hbosstats)
     histdata[e.first] = e.second.get_histogram();
   std::stringstream ss;
   {
     cereal::PortableBinaryOutputArchive wr(ss);
     wr(histdata);
   }
   return ss.str();
 }

 void HbosParam::deserialize_cerealpb(const std::string& parameters,  std::unordered_map<unsigned long, Histogram>& hbosstats)
 {
   std::stringstream ss; ss << parameters;
   std::unordered_map<unsigned long, Histogram::Data> histdata;
   {
     cereal::PortableBinaryInputArchive rd(ss);
     rd(histdata);
   }
   for(auto const& e : histdata)
     hbosstats[e.first] = Histogram::from_hist_data(e.second);
 }

 std::string HbosParam::update(const std::string& parameters, bool return_update)
 {
     std::unordered_map<unsigned long, Histogram> hbosstats;
     deserialize_cerealpb(parameters, hbosstats);
     if(return_update){
       update_and_return(hbosstats); //update hbosstats to reflect changes
       return serialize_cerealpb(hbosstats);
     }else{
       update(hbosstats);
       return "";
     }
 }

 void HbosParam::update(const std::unordered_map<unsigned long, Histogram>& hbosstats)
 {
     std::lock_guard<std::mutex> _(m_mutex);
     for (auto& pair: hbosstats) {
       verboseStream << "Histogram merge (no response) of func " << pair.first << std::endl;
       m_hbosstats[pair.first] += pair.second;
     }
 }

 void HbosParam::update_and_return(std::unordered_map<unsigned long, Histogram>& hbosstats)
 {
    std::lock_guard<std::mutex> _(m_mutex);
     for (auto& pair: hbosstats) {
       verboseStream << "Histogram merge (with response) of func " << pair.first << std::endl;
       m_hbosstats[pair.first] += pair.second;
       pair.second = hbosstats[pair.first];
     }

 }

 nlohmann::json HbosParam::get_algorithm_params(const unsigned long func_id) const{
   auto it = m_hbosstats.find(func_id);
   if(it == m_hbosstats.end()) throw std::runtime_error("Invalid function index in HbosParam::get_algorithm_params");
   return it->second.get_json();
 }


 bool HbosParam::find(const unsigned long func_id) const{ return m_hbosstats.find(func_id) != m_hbosstats.end(); }


 /**
  * @brief Histogram Class Implementation
  */


 Histogram::Histogram(){clear();}
 Histogram::~Histogram(){}

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
   double bin_width = Histogram::_scott_binWidth(g.counts(), g.bin_edges(), l.counts(), l.bin_edges());  /**< Compute bin width for merged histogram*/

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

   double start = min_runtime - 0.001*fabs(min_runtime); //start just below the first value because lower bounds are exclusive

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
  


double Histogram::_scott_binWidth(const std::vector<int> & global_counts, const std::vector<double> & global_edges, const std::vector<int> & local_counts, const std::vector<double> & local_edges){
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
   verboseStream << "Size in _scott_binWidth: " << size << std::endl;
   verboseStream << "Global sum in _scott_binWidth: " << sum << std::endl;

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
   verboseStream << "Total size in _scott_binWidth: " << size << std::endl;
   verboseStream << "Total sum in _scott_binWidth: " << sum << std::endl;

   if(size == 0) fatal_error("Sum of bin counts over both histograms is zero!");

   const double mean = sum / size;
   verboseStream << "mean in _xcott_binWidth: " << mean << std::endl;

   double var = 0.0, std=0.0;
   for (int i=0;i<global_counts.size();i++){
     var += global_counts.at(i) * pow(binValue(i, global_edges) - mean, 2);
   }
   verboseStream << "Global var in _scott_binWidth: " << var << std::endl;
   for (int i=0;i<local_counts.size();i++){
     var += local_counts.at(i) * pow(binValue(i, local_edges) - mean, 2);
   }
   verboseStream << "total var in _scott_binWidth: " << var << std::endl;

   var = var / size;
   verboseStream << "Final Variance in _scott_binWidth: " << var << std::endl;
   std = sqrt(var);
   verboseStream << "stddev in merging _scott_binWidth: " << std << std::endl;
   //if (std <= 100.0) {return 0;}

   return ((3.5 * std ) / pow(size, 1./3));

 }

 double Histogram::_scott_binWidth(const std::vector<double> & vals){
   //Find bin width as per Scott's rule = 3.5*std*n^-1/3

   double sum = std::accumulate(vals.begin(), vals.end(), 0.0);

   double mean = sum / vals.size();
   double var = 0.0, std = 0.0;
   for(int i=0; i<vals.size(); i++){
     var += pow(vals.at(i) - mean, 2);
   }
   var = var / vals.size();
   std = sqrt(var);
   verboseStream << "mean in _scott_binWidth: " << mean << std::endl;
   verboseStream << "stddev in _scott_binWidth: " << std << std::endl;

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

int Histogram::create_histogram(const std::vector<double>& r_times, double bin_width){
   if(r_times.size() == 0) fatal_error("No data points provided");

   //If there is only one data point or all data points have the same value we cannot use the Scott bin width rule because the std.dev is 0
   //Instead just put the bin edges +-1% around the point   
   bool all_same = true;
   for(size_t i=1;i<r_times.size();i++) if(r_times[i] != r_times[0]){ all_same =false; break; }
   
   if(all_same){
     double delta = 0.01 * fabs(r_times[0]);
     m_histogram.bin_edges.resize(2);
     m_histogram.bin_edges[0] = r_times[0] - delta;
     m_histogram.bin_edges[1] = r_times[0] + delta;
     m_histogram.counts.resize(1);
     m_histogram.counts[0] = r_times.size();
     verboseStream << "Histogram::create_histogram all data points have same value. Creating 1-bin histogram with edges " << m_histogram.bin_edges[0] << "," << m_histogram.bin_edges[1] << std::endl;
     return 0;
   }

   //Determine the bin width
   std::vector<double> runtimes = r_times;       
   if(bin_width == 0.){
     bin_width = Histogram::_scott_binWidth(runtimes);
     if (bin_width <= 0){
       fatal_error("Scott binwidth returned an invalid value "+std::to_string(bin_width));
     }
   }else{
     if(bin_width < 0.) fatal_error("Invalid bin width");
   }

   std::sort(runtimes.begin(), runtimes.end());
   const int h = runtimes.size() - 1;

   if (m_histogram.bin_edges.size() > 0) m_histogram.bin_edges.clear();

   double first_edge = runtimes[0] - 0.001 * fabs(runtimes[0]); //lower edges are exclusive and we want the first data point inside the first bin

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
   return 0;
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


 int Histogram::merge_histograms(const Histogram& g, const std::vector<double>& runtimes)
 {
   const int tot_size = runtimes.size() + std::accumulate(g.counts().begin(), g.counts().end(), 0);   
   std::vector<double> r_times(tot_size); // = runtimes;
   int idx = 0;
   verboseStream << "Number of runtime events during mergin: " << runtimes.size() << std::endl;
   verboseStream << "total number of 'g' bin_edges: " << g.bin_edges().size() << std::endl;

   //Fix for XGC run where unlabelled func_id is retained causing Zero bin_edges
   if (g.bin_edges().size() == 0)
     return this->create_histogram(runtimes);

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
   return this->create_histogram(r_times);
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



namespace chimbuko{
  std::ostream & operator<<(std::ostream &os, const Histogram &h){
    for(int i=0;i<h.counts().size();i++){
      os << h.bin_edges()[i] << "-" << h.bin_edges()[i+1] << ":" << h.counts()[i];
      if(i<h.counts().size()-1) os << std::endl;
    }
    return os;
  }
}
