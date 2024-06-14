#include "chimbuko/core/param/copod_param.hpp"
#include "chimbuko/core/param/hbos_param.hpp"
#include "chimbuko/core/util/error.hpp"
#include "chimbuko/core/verbose.hpp"
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

CopodFuncParam::CopodFuncParam():  m_internal_global_threshold(log2(1.00001)){}

nlohmann::json CopodFuncParam::get_json() const{
  nlohmann::json entry = nlohmann::json::object();
  entry["histogram"] = m_histogram.get_json();
  entry["internal_global_threshold"] = m_internal_global_threshold;
  return entry;
}

void CopodFuncParam::merge(const CopodFuncParam &other){
  m_histogram = Histogram::merge_histograms(m_histogram, other.m_histogram);
  m_internal_global_threshold = std::max(m_internal_global_threshold, other.m_internal_global_threshold);
}


 CopodParam::CopodParam() {
   clear();
 }

 CopodParam::~CopodParam(){

 }

 void CopodParam::clear()
 {
     m_copodstats.clear();
 }

 void CopodParam::show(std::ostream& os) const
 {
     os << "\n"
        << "Parameters: " << m_copodstats.size() << std::endl;

     for (auto stat: m_copodstats)
     {
         os << "Function " << stat.first << std::endl;
         os << stat.second.get_json().dump(2) << std::endl;
     }

     os << std::endl;
 }

 void CopodParam::assign(const CopodParam &other)
 {
   std::lock_guard<std::mutex> _(other.m_mutex);
   std::lock_guard<std::mutex> __(m_mutex);

   for (auto& pair: other.m_copodstats) {
     m_copodstats[pair.first] = pair.second;
   }
 }

 void CopodParam::assign(const std::string& parameters)
 {
   CopodParam tmp;
   deserialize_cerealpb(parameters, tmp);
   assign(tmp);
 }

 std::string CopodParam::serialize() const
 {
   std::lock_guard<std::mutex> _{m_mutex};   
   return serialize_cerealpb(*this);
 }

 std::string CopodParam::serialize_cerealpb(const CopodParam &param)
 {
   std::stringstream ss;
   {
     cereal::PortableBinaryOutputArchive wr(ss);
     wr(param.m_copodstats);
   }
   return ss.str();
 }

 void CopodParam::deserialize_cerealpb(const std::string& parameters,  CopodParam &p)
 {
   p.clear();
   std::stringstream ss; ss << parameters;
   {
     cereal::PortableBinaryInputArchive rd(ss);
     rd(p.m_copodstats);
   }
 }


nlohmann::json CopodParam::get_json() const{
  std::stringstream ss;
  {
    cereal::JSONOutputArchive wr(ss);
    wr(m_copodstats);
  }
  return nlohmann::json::parse(ss.str());
}  
  
void CopodParam::set_json(const nlohmann::json &from){
  std::stringstream ss; ss << from.dump();
  {
    cereal::JSONInputArchive rd(ss);
    rd(m_copodstats);
  }
}



 std::string CopodParam::update(const std::string& parameters, bool return_update)
 {
   CopodParam local_model;
   deserialize_cerealpb(parameters, local_model);
   if(return_update){
     update_and_return(local_model); //update copodstats to reflect changes
     return serialize_cerealpb(local_model);
   }else{
     update(local_model);
     return "";
   }
 }

void CopodParam::update_and_return(CopodParam &to_from){
   std::lock_guard<std::mutex> _(m_mutex);
   std::lock_guard<std::mutex> __(to_from.m_mutex);
   
   for (auto& pair: to_from.m_copodstats) {
     m_copodstats[pair.first].merge(pair.second);
     pair.second = m_copodstats[pair.first];
   }  
}

void CopodParam::update(const CopodParam& other) { 
  std::lock_guard<std::mutex> __(other.m_mutex);
  std::lock_guard<std::mutex> _(m_mutex);
  for (auto& pair: other.m_copodstats) {
    m_copodstats[pair.first].merge(pair.second);
  }
}


 nlohmann::json CopodParam::get_algorithm_params(const unsigned long model_idx) const{
   auto it = m_copodstats.find(model_idx);
   if(it == m_copodstats.end()) throw std::runtime_error("Invalid function index in CopodParam::get_algorithm_params");
   return it->second.get_json();
 }

std::unordered_map<unsigned long, nlohmann::json> CopodParam::get_all_algorithm_params() const{
  std::unordered_map<unsigned long, nlohmann::json> out;
  for(auto const &r : m_copodstats)
    out[r.first] = r.second.get_json();
  return out;
}


 const int CopodParam::find(const unsigned long& func_id) {
   if(m_copodstats.find(func_id) == m_copodstats.end()) { // func_id not in map
     return 0;
   }
   return 1;
 }
