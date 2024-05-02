#include "chimbuko/core/param/hbos_param.hpp"
#include "chimbuko/verbose.hpp"
#include "chimbuko/core/util/error.hpp"
#include <sstream>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/archives/json.hpp>
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
HbosFuncParam::HbosFuncParam():  m_internal_global_threshold(log2(1.00001)){}


nlohmann::json HbosFuncParam::get_json() const{
  nlohmann::json entry = nlohmann::json::object();
  entry["internal_global_threshold"] = m_internal_global_threshold;
  entry["histogram"] = m_histogram.get_json();
  return entry;
}

void HbosFuncParam::merge(const HbosFuncParam &other, const binWidthSpecifier &bw){
  m_histogram = Histogram::merge_histograms(m_histogram, other.m_histogram, bw);
  m_internal_global_threshold = std::max(m_internal_global_threshold, other.m_internal_global_threshold); //choose the more stringent threshold
}

HbosParam::HbosParam(): m_maxbins(200){
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

nlohmann::json HbosParam::get_json() const{
  std::stringstream ss;
  {
    cereal::JSONOutputArchive wr(ss);
    wr(m_hbosstats);
    wr(m_maxbins);
  }
  return nlohmann::json::parse(ss.str());
}  
  
void HbosParam::set_json(const nlohmann::json &from){
  std::stringstream ss; ss << from.dump();
  {
    cereal::JSONInputArchive rd(ss);
    rd(m_hbosstats);
    rd(m_maxbins);
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

void HbosParam::update_internal(const HbosParam &from){
  m_maxbins = from.m_maxbins; //copy the max number of bins from the input data
  binWidthFixedNbin bwspec(m_maxbins);    
  
  for (auto& pair: from.m_hbosstats) {
    verboseStream << "Histogram merge of func " << pair.first << std::endl;   
    m_hbosstats[pair.first].merge(pair.second, bwspec);
  }
} 

 void HbosParam::update(const HbosParam &from)
 {
   std::lock_guard<std::mutex> _(from.m_mutex);
   std::lock_guard<std::mutex> __(m_mutex);
   this->update_internal(from);
 }

 void HbosParam::update_and_return(HbosParam &to_from)
 {
   std::lock_guard<std::mutex> _(to_from.m_mutex);
   std::lock_guard<std::mutex> __(m_mutex);
   this->update_internal(to_from); //update this
   for (auto& pair: to_from.m_hbosstats)
     pair.second = m_hbosstats[pair.first];
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


void HbosParam::generate_histogram(const unsigned long func_id, const std::vector<double> &runtimes, double global_threshold_init, HbosParam const *global_param){
  if (runtimes.size() > 0) {
    verboseStream << "Creating local histogram for func " << func_id << " for " << runtimes.size() << " data points" << std::endl;

    Histogram const* global_hist = nullptr;
    if(global_param != nullptr){
      auto hit = global_param->get_hbosstats().find(func_id);
      if(hit != global_param->get_hbosstats().end()) global_hist = &hit->second.getHistogram();
    }

    HbosFuncParam &fparam = m_hbosstats[func_id];
    Histogram &hist = fparam.getHistogram();

    //Always use the max number of bins for finest resolution
    binWidthFixedNbin bwspec(m_maxbins);
    hist.create_histogram(runtimes, bwspec);

    fparam.setInternalGlobalThreshold(global_threshold_init);

    verboseStream << "Function " << func_id << " generated histogram has " << hist.counts().size() << " bins:" << std::endl;
    verboseStream << hist << std::endl;
  }
}


bool HbosParam::operator==(const HbosParam &r) const{
  for(auto it = r.m_hbosstats.begin(); it != r.m_hbosstats.end(); ++it){
    auto lit = this->m_hbosstats.find(it->first);
    if(lit == this->m_hbosstats.end()) return false;
    if(lit->second != it->second) return false;
  }
  return true;
}
