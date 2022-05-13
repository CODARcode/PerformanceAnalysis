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
HbosParam::HbosParam(): m_maxbins(std::numeric_limits<int>::max()){
   clear();
 }

 HbosParam::~HbosParam(){

 }

 void HbosParam::copy(const HbosParam &r){ 
   std::lock_guard<std::mutex> _(r.m_mutex);
   std::lock_guard<std::mutex> __(m_mutex);
   m_hbosstats = r.m_hbosstats; 
   m_maxbins = r.m_maxbins;
 }


 void HbosParam::clear()
 {
   std::lock_guard<std::mutex> _(m_mutex);
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

 void HbosParam::assign(const HbosParam &other)
 {
   std::lock_guard<std::mutex> _(other.m_mutex);
   std::lock_guard<std::mutex> __(m_mutex);

   for (auto& pair: other.m_hbosstats) {
     m_hbosstats[pair.first] = pair.second;
   }
 }

 void HbosParam::assign(const std::string& parameters)
 {
   HbosParam tmp;
   deserialize_cerealpb(parameters, tmp);
   assign(tmp);
 }

 std::string HbosParam::serialize() const
 {
   std::lock_guard<std::mutex> _{m_mutex};
   return serialize_cerealpb(*this);
 }

 std::string HbosParam::serialize_cerealpb(const HbosParam &p)
 {
   std::stringstream ss;
   {
     cereal::PortableBinaryOutputArchive wr(ss);
     wr(p.m_hbosstats);
     wr(p.m_maxbins);
   }
   return ss.str();
 }

 void HbosParam::deserialize_cerealpb(const std::string& parameters, HbosParam &p)
 {
   p.clear();
   std::stringstream ss; ss << parameters;

   {
     cereal::PortableBinaryInputArchive rd(ss);
     rd(p.m_hbosstats);
     rd(p.m_maxbins);
   }
 }

 std::string HbosParam::update(const std::string& parameters, bool return_update)
 {
   HbosParam local_model;
   deserialize_cerealpb(parameters, local_model);
   if(return_update){
     update_and_return(local_model); //update hbosstats to reflect changes
     return serialize_cerealpb(local_model);
   }else{
     update(local_model);
     return "";
   }
 }

 void HbosParam::update(const HbosParam &from)
 {
   std::lock_guard<std::mutex> _(from.m_mutex);
   std::lock_guard<std::mutex> __(m_mutex);
   m_maxbins = from.m_maxbins; //copy the max number of bins from the input data
   binWidthScottMaxNbin bwspec(m_maxbins); //use Scott method to set the bin width
   
   for (auto& pair: from.m_hbosstats) {
     verboseStream << "Histogram merge (no response) of func " << pair.first << std::endl;
     m_hbosstats[pair.first] = Histogram::merge_histograms(m_hbosstats[pair.first], pair.second, bwspec);
   }
 }

 void HbosParam::update_and_return(HbosParam &to_from)
 {
   std::lock_guard<std::mutex> _(to_from.m_mutex);
   std::lock_guard<std::mutex> __(m_mutex);
   m_maxbins = to_from.m_maxbins; //copy the max number of bins from the input data
   binWidthScottMaxNbin bwspec(m_maxbins); //use Scott method to set the bin width

   for (auto& pair: to_from.m_hbosstats) {
     verboseStream << "Histogram merge (with response) of func " << pair.first << std::endl;
     m_hbosstats[pair.first] = Histogram::merge_histograms(m_hbosstats[pair.first], pair.second, bwspec);
     pair.second = m_hbosstats[pair.first];
   }

 }


 nlohmann::json HbosParam::get_algorithm_params(const unsigned long func_id) const{
   auto it = m_hbosstats.find(func_id);
   if(it == m_hbosstats.end()) throw std::runtime_error("Invalid function index in HbosParam::get_algorithm_params");
   return it->second.get_json();
 }

nlohmann::json HbosParam::get_algorithm_params(const std::unordered_map<unsigned long, std::pair<unsigned long, std::string> > & func_id_map) const{
  nlohmann::json out = nlohmann::json::array();
  for(auto const &r : m_hbosstats){
    auto fit = func_id_map.find(r.first);
    if(fit == func_id_map.end()) fatal_error("Could not find function in input map");
    nlohmann::json entry = nlohmann::json::object();
    entry["fid"] = r.first;
    entry["pid"] = fit->second.first;
    entry["func_name"] = fit->second.second;
    entry["model"] = r.second.get_json();
    out.push_back(std::move(entry));
  }
  return out;
}



 bool HbosParam::find(const unsigned long func_id) const{ return m_hbosstats.find(func_id) != m_hbosstats.end(); }


void HbosParam::generate_histogram(const unsigned long func_id, const std::vector<double> &runtimes, double hbos_threshold, HbosParam const *global_param){
  if (runtimes.size() > 0) {
    verboseStream << "Creating local histogram for func " << func_id << " for " << runtimes.size() << " data points" << std::endl;

    Histogram const* global_hist = nullptr;
    if(global_param != nullptr){
      auto hit = global_param->get_hbosstats().find(func_id);
      if(hit != global_param->get_hbosstats().end()) global_hist = &hit->second;
    }

    Histogram &hist = m_hbosstats[func_id];
    if(global_hist != nullptr){
      //Choose a bin width based on a combination of the local dataset and the existing global model to reduce discretization errors from merging a coarse and fine histogram
      double bw = Histogram::scottBinWidth(global_hist->counts(), global_hist->bin_edges(), runtimes);
      verboseStream << "Combining knowledge of current global model and local dataset, chose bin width " << bw << std::endl;
      binWidthFixedMaxNbin l_bwspec(bw, m_maxbins);
      hist.create_histogram(runtimes, l_bwspec);
    }else{
      verboseStream << "Using Scott bin width of local data set to determine bin width" << std::endl;
      binWidthScottMaxNbin l_bwspec(m_maxbins);
      hist.create_histogram(runtimes, l_bwspec);
    }
    hist.set_glob_threshold(hbos_threshold);

    verboseStream << "Function " << func_id << " generated histogram has " << hist.counts().size() << " bins:" << std::endl;
    verboseStream << hist << std::endl;
  }
}
