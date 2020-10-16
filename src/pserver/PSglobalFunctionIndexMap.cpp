#include <chimbuko/pserver/PSglobalFunctionIndexMap.hpp>
#include <algorithm>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

using namespace chimbuko;

unsigned long PSglobalFunctionIndexMap::lookup(const std::string &func_name){
  static unsigned long idx = 0;
  std::lock_guard<std::mutex> _(m_mutex);
  auto it = m_fmap.find(func_name);
  if(it == m_fmap.end()){
    unsigned long fidx = m_idx++;
    m_fmap[func_name] = fidx;
    return fidx;
  }
  else return it->second;
}
    
bool PSglobalFunctionIndexMap::contains(const std::string &func_name) const{
  std::lock_guard<std::mutex> _(m_mutex);
  return m_fmap.find(func_name) != m_fmap.end();
}

nlohmann::json PSglobalFunctionIndexMap::serialize() const{
  std::lock_guard<std::mutex> _(m_mutex);
  return nlohmann::json(m_fmap);
}

void PSglobalFunctionIndexMap::deserialize(const nlohmann::json &fmap){
  std::lock_guard<std::mutex> _(m_mutex);
  m_fmap = fmap.get< std::unordered_map<std::string, unsigned long> >();
  
  //Set the next index to 1 after the largest previously-assigned value
  unsigned long max = 0;
  for(auto const &e : m_fmap)
    max = std::max(max, e.second);
  m_idx = max+1;
}

void NetPayloadGlobalFunctionIndexMap::action(Message &response, const Message &message){
  check(message);
  if(m_idxmap == nullptr) throw std::runtime_error("Cannot retrieve function index as map object has not been linked");
  unsigned long idx = m_idxmap->lookup(message.buf()); //uses a mutex lock
  response.set_msg(anyToStr(idx), false);
}


void NetPayloadGlobalFunctionIndexMapBatched::action(Message &response, const Message &message){
  check(message);
  if(m_idxmap == nullptr) throw std::runtime_error("Cannot retrieve function index as map object has not been linked");

  std::vector<std::string> func_names;
  {
    std::stringstream ss; ss << message.buf();
    cereal::PortableBinaryInputArchive rd(ss);
    rd(func_names);
  }    
  std::vector<unsigned long> global_indices(func_names.size());
  for(size_t i=0;i<func_names.size();i++)
    global_indices[i] = m_idxmap->lookup(func_names[i]); //uses a mutex lock

  {
    std::stringstream ss;
    cereal::PortableBinaryOutputArchive wr(ss);
    wr(global_indices);
    response.set_msg(ss.str(), false);
  }    
}

