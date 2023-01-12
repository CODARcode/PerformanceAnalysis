#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/util/error.hpp"
#include <sstream>
#include <cereal/archives/portable_binary.hpp>
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


nlohmann::json SstdParam::get_algorithm_params(const unsigned long func_id) const{
  auto it = m_runstats.find(func_id);
  if(it == m_runstats.end()) throw std::runtime_error("Invalid function index in SstdParam::get_algorithm_params");
  return it->second.get_json();
}

nlohmann::json SstdParam::get_algorithm_params(const std::unordered_map<unsigned long, std::pair<unsigned long, std::string> > & func_id_map) const{
  nlohmann::json out = nlohmann::json::array();
  for(auto const &r : m_runstats){
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
