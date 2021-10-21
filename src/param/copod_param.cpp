#include "chimbuko/param/copod_param.hpp"
#include "chimbuko/param/hbos_param.hpp"
#include "chimbuko/verbose.hpp"
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

 void CopodParam::assign(const std::unordered_map<unsigned long, Histogram>& copodstats)
 {
     std::lock_guard<std::mutex> _(m_mutex);
     for (auto& pair: copodstats) {
         m_copodstats[pair.first] = pair.second;
     }
 }

 void CopodParam::assign(const std::string& parameters)
 {
     std::unordered_map<unsigned long, Histogram> copodstats;

     deserialize_cerealpb(parameters, copodstats);
     assign(copodstats);
 }

 std::string CopodParam::serialize() const
 {
     std::lock_guard<std::mutex> _{m_mutex};

     return serialize_cerealpb(m_copodstats);
 }

 std::string CopodParam::serialize_cerealpb(const std::unordered_map<unsigned long, Histogram>& copodstats)
 {
   std::unordered_map<unsigned long, Histogram::Data> histdata;
   for(auto const& e : copodstats)
     histdata[e.first] = e.second.get_histogram();
   std::stringstream ss;
   {
     cereal::PortableBinaryOutputArchive wr(ss);
     wr(histdata);
   }
   return ss.str();
 }

 void CopodParam::deserialize_cerealpb(const std::string& parameters,  std::unordered_map<unsigned long, Histogram>& copodstats)
 {
   std::stringstream ss; ss << parameters;
   std::unordered_map<unsigned long, Histogram::Data> histdata;
   {
     cereal::PortableBinaryInputArchive rd(ss);
     rd(histdata);
   }
   for(auto const& e : histdata)
     copodstats[e.first] = Histogram::from_hist_data(e.second);
 }

 std::string CopodParam::update(const std::string& parameters, bool return_update)
 {
     std::unordered_map<unsigned long, Histogram> copodstats;
     deserialize_cerealpb(parameters, copodstats);
     if(return_update){
       update_and_return(copodstats); //update copodstats to reflect changes
       return serialize_cerealpb(copodstats);
     }else{
       update(copodstats);
       return "";
     }
 }

 void CopodParam::update(const std::unordered_map<unsigned long, Histogram>& copodstats)
 {
     std::lock_guard<std::mutex> _(m_mutex);
     for (auto& pair: copodstats) {
         m_copodstats[pair.first] += pair.second;
     }
 }

 void CopodParam::update_and_return(std::unordered_map<unsigned long, Histogram>& copodstats)
 {
    std::lock_guard<std::mutex> _(m_mutex);
     for (auto& pair: copodstats) {
         m_copodstats[pair.first] += pair.second;
         pair.second = copodstats[pair.first];
     }

 }

 nlohmann::json CopodParam::get_algorithm_params(const unsigned long func_id) const{
   auto it = m_copodstats.find(func_id);
   if(it == m_copodstats.end()) throw std::runtime_error("Invalid function index in CopodParam::get_algorithm_params");
   return it->second.get_json();
 }

 const int CopodParam::find(const unsigned long& func_id) {
   if(m_copodstats.find(func_id) == m_copodstats.end()) { // func_id not in map
     return 0;
   }
   return 1;
 }
