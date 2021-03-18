#include "chimbuko/param/hbos_param.hpp"
#include <sstream>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/access.hpp>
#include <cereal/types/vector.hpp>
#include <limits>
#include <cmath>

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
         //Histogram::Data d = pair.second.get_histogram();
         //m_hbosstats[pair.first].set_hist_data(d);
     }
 }

 void HbosParam::assign(const std::string& parameters)
 {
     std::unordered_map<unsigned long, Histogram> hbosstats;
     //deserialize_json(parameters, runstats);
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
     update_and_return(hbosstats); //update runstats to reflect changes

     return (return_update) ? serialize_cerealpb(hbosstats): "";
 }

 void HbosParam::update(const std::unordered_map<unsigned long, Histogram>& hbosstats)
 {
     std::lock_guard<std::mutex> _(m_mutex);
     for (auto& pair: hbosstats) {
         m_hbosstats[pair.first] += pair.second;
     }
 }

 void HbosParam::update_and_return(std::unordered_map<unsigned long, Histogram>& hbosstats)
 {
     std::lock_guard<std::mutex> _(m_mutex);
     for (auto& pair: hbosstats) {
         m_hbosstats[pair.first] += pair.second;
         pair.second = m_hbosstats[pair.first];
     }
 }

 nlohmann::json HbosParam::get_algorithm_params(const unsigned long func_id) const{
   auto it = m_hbosstats.find(func_id);
   if(it == m_hbosstats.end()) throw std::runtime_error("Invalid function index in SstdParam::get_function_stats");
   return it->second.get_json();
 }

 const int HbosParam::find(const unsigned long& func_id) {
   if(m_hbosstats.find(func_id) == m_hbosstats.end()) { // func_id not in map
     return 0;
   }
   return 1;
 }


 /**
  * @brief Histogram Class Implementation
  */

  /**
   * @brief Merge Histogram
   */
 Histogram::Histogram(){}
 Histogram::~Histogram(){}

 Histogram & Histogram::combine_two_histograms (const Histogram& g, const Histogram& l) const{
   Histogram combined;
   double min_runtime = std::numeric_limits<double>::max(), max_runtime = 0;
   std::cout << "Bin_Edges Size of Global Histogram: " << std::to_string(g.bin_edges().size()) << ", Bin_Edges Size of Local Histogram: " << std::to_string(l.bin_edges().size()) << std::endl;
   std::cout << "Counts Size of Global Histogram: " << std::to_string(g.counts().size()) << ", Counts Size of Local Histogram: " << std::to_string(l.counts().size()) << std::endl;
   if (g.bin_edges().size() <= 1) { //== 0) {
     combined.set_glob_threshold(l.get_threshold());
     combined.set_counts(l.counts());
     combined.set_bin_edges(l.bin_edges());
   }
   else if (l.bin_edges().size() <= 1) { //== 0) {
     combined.set_glob_threshold(g.get_threshold());
     combined.set_counts(g.counts());
     combined.set_bin_edges(g.bin_edges());
   }
   else {

     const double bin_width = Histogram::_scott_binWidth(g.counts(), g.bin_edges(), l.counts(), l.bin_edges());
     //std::cout << "BIN WIDTH while merging: " << bin_width << std::endl;

     combined.add2binedges(min_runtime);
     //std::cout << "Minimum Runtime: " << std::to_string(min_runtime) << ", Maximum Runtime: " << std::to_string(max_runtime) << std::endl;
     //std::cout << "Combined Bin Edges: [" << std::to_string(min_runtime) << ", " ;
     double prev = min_runtime;
     while(prev < max_runtime) {
       const double b = bin_width + prev;
       //std::cout << std::to_string(b) << ", ";
       combined.add2binedges(b);
       prev = b;

     }
     //std::cout << "]" << std::endl;

     //verboseStream << "Number of bins: " << combined.data.bin_edges.size()-1 << std::endl;
     std::vector<int> count = std::vector<int>(combined.bin_edges().size()-1, 0);
     combined.set_counts(count);
     for (int i = 0; i < g.bin_edges().size() -1; i++) {
       for(int j = 1; j < combined.bin_edges().size(); j++) {
         if(g.bin_edges().at(i) < combined.bin_edges().at(j)) {
           int id = j-1, inc = g.counts().at(i);
           combined.add2counts(id, inc);
           break;
         }
       }
     }

     for (int i = 0; i < l.bin_edges().size() -1; i++) {
       for(int j = 1; j < combined.bin_edges().size(); j++) {
         if(l.bin_edges().at(i) < combined.bin_edges().at(j)) {
           int id = j-1, inc = l.counts().at(i);
           combined.add2counts(id, inc);
           break;
         }
       }
     }

     if(l.get_threshold() > g.get_threshold())
      combined.set_glob_threshold(l.get_threshold());
     else
      combined.set_glob_threshold(g.get_threshold());

     //const Histogram::Data d_tmp(combined.get_threshold(), combined.counts(), combined.bin_edges() );

     return &combined;
   }
 }

 Histogram& Histogram::operator+=(const Histogram& h)
 {
    const Histogram combined = combine_two_histograms(*this, h);
    *this = combined;
    this->set_hist_data(Histogram::Data(this->get_threshold(), this->counts(), this->bin_edges()));
    return *this;
 }

 double Histogram::_scott_binWidth(const std::vector<int> & global_counts, const std::vector<double> & global_edges, const std::vector<int> & local_counts, const std::vector<double> & local_edges){
   double sum = 0.0;
   const double size = (double) (std::accumulate(global_counts.begin(), global_counts.end(), 0) + std::accumulate(local_counts.begin(), local_counts.end(), 0));
   for (int i=0;i<global_counts.size();i++){
     sum += global_counts.at(i) * global_edges.at(i);
   }
   for (int i=0;i<local_counts.size();i++){
     sum += local_counts.at(i) * local_edges.at(i);
   }

   const double mean = sum / size;
   double var = 0.0;
   for (int i=0;i<global_counts.size();i++){
     var += global_counts.at(i) * pow((global_edges.at(i) - mean), 2);
   }
   for (int i=0;i<local_counts.size();i++){
     var += local_counts.at(i) * pow((local_edges.at(i) - mean), 2);
   }
   var = var / size;
   return ((3.5 * sqrt(var) ) / pow(size, 1/3));
 }

 double Histogram::_scott_binWidth(const std::vector<double> & vals){
   //Find bin width as per Scott's rule = 3.5*std*n^-1/3

   double sum = std::accumulate(vals.begin(), vals.end(), 0.0);

   double mean = sum / vals.size();
   double var = 0.0;
   for(int i=0; i<vals.size(); i++){
     var += pow(vals.at(i) - mean, 2);
   }
   var = var / vals.size();

   return ((3.5 * sqrt(var) ) / pow(vals.size(), 1/3));
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

 void Histogram::create_histogram(std::vector<double>& runtimes)
 {

   const double bin_width = Histogram::_scott_binWidth(runtimes);
   std::sort(runtimes.begin(), runtimes.end());
   const int h = runtimes.size() - 1;

   if (m_histogram.bin_edges.size() > 0) m_histogram.bin_edges.clear();

   m_histogram.bin_edges.push_back(runtimes.at(0));

   double prev = m_histogram.bin_edges.at(0);
   while(prev < runtimes.at(h)){
     m_histogram.bin_edges.push_back(prev + bin_width);
     prev += bin_width;
   }
   //verboseStream << "Number of bins: " << m_histogram.bin_edges.size()-1 << std::endl;

   if (m_histogram.counts.size() > 0) m_histogram.counts.clear();
   m_histogram.counts = std::vector<int>(m_histogram.bin_edges.size()-1, 0);
   for ( int i=0; i < runtimes.size(); i++) {
     for ( int j=1; j < m_histogram.bin_edges.size(); j++) {
       if ( runtimes.at(i) < m_histogram.bin_edges.at(j) ) {
         m_histogram.counts[j-1] += 1;
         break;
       }
     }
   }
   //verboseStream << "Size of counts: " << m_histogram.counts.size() << std::endl;

   //m_histogram.runtimes.clear();
   const double min_threshold = -1 * log2(1.00001);
   if (!(m_histogram.glob_threshold > min_threshold)) {
     m_histogram.glob_threshold = min_threshold;
   }
   this->set_hist_data(Histogram::Data( m_histogram.glob_threshold, m_histogram.counts, m_histogram.bin_edges ));

 }

 void Histogram::merge_histograms(Histogram& g, const std::vector<double>& runtimes)
 {
   //Histogram merged_h;
   std::vector<double> r_times = runtimes;

   for (int i = 0; i < g.bin_edges().size() - 1; i++) {
     for(int j = 0; j < g.counts().at(i); j++){
       r_times.push_back(g.bin_edges().at(i));
     }
   }

   m_histogram.glob_threshold = g.get_threshold();
   std::cout << "glob_threshold in merge_histograms = " << m_histogram.glob_threshold << std::endl;
   this->create_histogram(r_times);
   //this->set_hist_data(Histogram::Data( m_histogram.glob_threshold, m_histogram.counts, m_histogram.bin_edges ));

 }

 nlohmann::json Histogram::get_json() const {
     return {
         {"Histogram Bin Counts", m_histogram.counts},
         {"Histogram Bin Edges", m_histogram.bin_edges}};
 }
