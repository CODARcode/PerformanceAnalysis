#include <chimbuko/pserver/PSglobalFunctionIndexMap.hpp>
#include <algorithm>

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

