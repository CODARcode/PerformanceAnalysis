#include "chimbuko/core/param/sstd_param.hpp"
#include "chimbuko/core/util/error.hpp"
#include <sstream>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/access.hpp>
#include <cereal/types/vector.hpp>
#include <limits>
#include <cmath>

using namespace chimbuko;

SstdParam::SstdParam()
{
    clear();
}

SstdParam::~SstdParam()
{

}

std::string SstdParam::serialize() const
{
    std::lock_guard<std::mutex> _{m_mutex};
    return serialize_cerealpb(m_runstats);
}

std::string SstdParam::serialize_cerealpb(const std::unordered_map<unsigned long, RunStats>& runstats){
  std::stringstream ss;
  {
    cereal::PortableBinaryOutputArchive wr(ss);
    wr(runstats);
  }
  return ss.str();
}

void SstdParam::deserialize_cerealpb(const std::string& parameters,  std::unordered_map<unsigned long, RunStats>& runstats){
  std::stringstream ss; ss << parameters;
  {
    cereal::PortableBinaryInputArchive rd(ss);
    rd(runstats);
  }
}

nlohmann::json SstdParam::serialize_json() const{
  std::stringstream ss;
  {
    cereal::JSONOutputArchive wr(ss);
    wr(m_runstats);
  }
  return nlohmann::json::parse(ss.str());
}  
  
void SstdParam::deserialize_json(const nlohmann::json &from){
  std::stringstream ss; ss << from.dump();
  {
    cereal::JSONInputArchive rd(ss);
    rd(m_runstats);
  }
}


std::string SstdParam::update(const std::string& parameters, bool return_update)
{
    std::unordered_map<unsigned long, RunStats> runstats;
    deserialize_cerealpb(parameters, runstats);
    if(return_update){
      update_and_return(runstats);  //update runstats to reflect changes
      return serialize_cerealpb(runstats);
    }else{
      update(runstats);
      return "";
    }
}

void SstdParam::assign(const std::unordered_map<unsigned long, RunStats>& runstats)
{
    std::lock_guard<std::mutex> _(m_mutex);
    for (auto& pair: runstats) {
        m_runstats[pair.first] = pair.second;
    }
}

void SstdParam::assign(const std::string& parameters)
{
    std::unordered_map<unsigned long, RunStats> runstats;

    deserialize_cerealpb(parameters, runstats);
    assign(runstats);
}

void SstdParam::clear()
{
    m_runstats.clear();
}


void SstdParam::update(const std::unordered_map<unsigned long, RunStats>& runstats)
{
    std::lock_guard<std::mutex> _(m_mutex);
    for (auto& pair: runstats) {
        m_runstats[pair.first] += pair.second;
    }
}


void SstdParam::update_and_return(std::unordered_map<unsigned long, RunStats>& runstats)
{
    std::lock_guard<std::mutex> _(m_mutex);
    for (auto& pair: runstats) {
        m_runstats[pair.first] += pair.second;
        pair.second = m_runstats[pair.first];
    }
}

void SstdParam::update(const SstdParam& other) { 
  std::lock_guard<std::mutex> _(other.m_mutex);  
  update(other.m_runstats); 
}


void SstdParam::show(std::ostream& os) const
{
    os << "\n"
       << "Parameters: " << m_runstats.size() << std::endl;

    for (auto stat: m_runstats)
    {
        os << "Function " << stat.first << std::endl;
        os << stat.second.get_json().dump(2) << std::endl;
    }

    os << std::endl;
}


nlohmann::json SstdParam::get_algorithm_params(const unsigned long model_idx) const{
  auto it = m_runstats.find(model_idx);
  if(it == m_runstats.end()) throw std::runtime_error("Invalid model index in SstdParam::get_algorithm_params");
  return it->second.get_json();
}

std::unordered_map<unsigned long, nlohmann::json> SstdParam::get_all_algorithm_params() const{
  std::unordered_map<unsigned long, nlohmann::json> out;
  for(auto const &r : m_runstats)
    out[r.first] = r.second.get_json();
  return out;
}
