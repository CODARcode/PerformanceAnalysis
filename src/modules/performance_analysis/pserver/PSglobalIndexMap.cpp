#include <chimbuko/modules/performance_analysis/pserver/PSglobalIndexMap.hpp>
#include <algorithm>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/utility.hpp>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

unsigned long PSglobalIndexMap::lookup(unsigned long pid, const std::string &name){
  std::lock_guard<std::mutex> _(m_mutex);
  auto pit = m_fmap.find(pid);

  if(pit == m_fmap.end())
    pit = m_fmap.emplace(pid, std::unordered_map<std::string, unsigned long>()).first;

  auto & fmap_pid = pit->second;

  auto it = fmap_pid.find(name);
  if(it == fmap_pid.end()){
    unsigned long fidx = m_idx++;
    fmap_pid[name] = fidx;
    return fidx;
  }
  else return it->second;
}
    
bool PSglobalIndexMap::contains(unsigned long pid, const std::string &name) const{
  std::lock_guard<std::mutex> _(m_mutex);
  auto pit = m_fmap.find(pid);
  if(pit == m_fmap.end()) return false;
  else return pit->second.find(name) != pit->second.end();
}

nlohmann::json PSglobalIndexMap::serialize() const{
  std::lock_guard<std::mutex> _(m_mutex);
  return nlohmann::json(m_fmap);
}

void PSglobalIndexMap::deserialize(const nlohmann::json &fmap){
  std::lock_guard<std::mutex> _(m_mutex);
  m_fmap = fmap.get< std::unordered_map<unsigned long, std::unordered_map<std::string, unsigned long> > >();
  
  //Set the next index to 1 after the largest previously-assigned value
  unsigned long max = 0;
  for(auto const &p : m_fmap)
    for(auto const &e : p.second)
      max = std::max(max, e.second);
  m_idx = max+1;
}


std::unordered_map<unsigned long, std::pair<unsigned long, std::string> > PSglobalIndexMap::getIndexMap() const{
  std::unordered_map<unsigned long, std::pair<unsigned long, std::string> > out;
  for(auto const &p : m_fmap)
    for(auto const &f : p.second)
      out[f.second] = {p.first, f.first};
  return out;
}



void NetPayloadGlobalIndexMap::action(Message &response, const Message &message){
  check(message);
  if(m_idxmap == nullptr) throw std::runtime_error("Cannot retrieve function index as map object has not been linked");

  std::pair<unsigned long, std::string> pid_fname;
  {
    std::stringstream ss; ss << message.getContent();
    cereal::PortableBinaryInputArchive rd(ss);
    rd(pid_fname);
  }
  unsigned long idx = m_idxmap->lookup(pid_fname.first, pid_fname.second); //uses a mutex lock
  response.setContent(anyToStr(idx));
}


void NetPayloadGlobalIndexMapBatched::action(Message &response, const Message &message){
  check(message);
  if(m_idxmap == nullptr) throw std::runtime_error("Cannot retrieve function index as map object has not been linked");

  unsigned long pid;
  std::vector<std::string> func_names;
  {
    std::stringstream ss; ss << message.getContent();
    cereal::PortableBinaryInputArchive rd(ss);
    rd(pid);
    rd(func_names);
  }    
  std::vector<unsigned long> global_indices(func_names.size());
  for(size_t i=0;i<func_names.size();i++)
    global_indices[i] = m_idxmap->lookup(pid, func_names[i]); //uses a mutex lock

  {
    std::stringstream ss;
    cereal::PortableBinaryOutputArchive wr(ss);
    wr(global_indices);
    response.setContent(ss.str());
  }    
}


