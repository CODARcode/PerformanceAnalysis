#include "chimbuko/param/sstd_param.hpp"
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

std::string SstdParam::serialize_json(const std::unordered_map<unsigned long, RunStats>& runstats)
{
    nlohmann::json j;
    for (auto& pair: runstats)
    {
        j[std::to_string(pair.first)] = pair.second.get_json_state();
    }
    return j.dump();
}

void SstdParam::deserialize_json(
    const std::string& parameters,
    std::unordered_map<unsigned long, RunStats>& runstats)
{
    nlohmann::json j = nlohmann::json::parse(parameters);

    for (auto it = j.begin(); it != j.end(); it++)
    {
        unsigned long key = std::stoul(it.key());
        runstats[key] = RunStats::from_strstate(it.value().dump());
    }
}

std::string SstdParam::serialize_cerealpb(const std::unordered_map<unsigned long, RunStats>& runstats){
  std::unordered_map<unsigned long, RunStats::State> state;
  for(auto const& e : runstats)
    state[e.first] = e.second.get_state();
  std::stringstream ss;
  {
    cereal::PortableBinaryOutputArchive wr(ss);
    wr(state);
  }
  return ss.str();
}

void SstdParam::deserialize_cerealpb(const std::string& parameters,  std::unordered_map<unsigned long, RunStats>& runstats){
  std::stringstream ss; ss << parameters;
  std::unordered_map<unsigned long, RunStats::State> state;
  {
    cereal::PortableBinaryInputArchive rd(ss);
    rd(state);
  }
  for(auto const& e : state)
    runstats[e.first] = RunStats::from_state(e.second);
}


std::string SstdParam::update(const std::string& parameters, bool return_update)
{
    std::unordered_map<unsigned long, RunStats> runstats;

    deserialize_cerealpb(parameters, runstats);
    update_and_return(runstats);

    return (return_update) ? serialize_cerealpb(runstats): "";
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
