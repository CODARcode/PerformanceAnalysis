#include <chimbuko/ad/ADMetadataParser.hpp>
#include <chimbuko/util/string.hpp>
#include <regex>

using namespace chimbuko;

GPUvirtualThreadInfo & getGPUthreadInfo(const unsigned long virtual_thread, std::unordered_map<unsigned long, GPUvirtualThreadInfo> &map){
  auto it = map.find(virtual_thread);
  if(it == map.end()) 
    it = map.emplace(virtual_thread, GPUvirtualThreadInfo(virtual_thread,-1,-1,0) ).first;
  return it->second;
}


void ADMetadataParser::parseMetadata(const MetaData_t &m){
  std::smatch match;
  if(m.get_descr() == "CUDA Context"){
    unsigned long virtual_thread = m.get_tid();
    getGPUthreadInfo(m.get_tid(), m_gpu_thread_map).context = strToAny<int>(m.get_value());
  }else if(m.get_descr() == "CUDA Stream"){
    getGPUthreadInfo(m.get_tid(), m_gpu_thread_map).stream = strToAny<int>(m.get_value());
  }else if(m.get_descr() == "CUDA Device"){
    getGPUthreadInfo(m.get_tid(), m_gpu_thread_map).device = strToAny<int>(m.get_value());
  }else if(std::regex_match(m.get_descr(), match, std::regex(R"(GPU\[(\d+)\]\s(.*))")) ){
    int device = strToAny<int>(match[1]);
    std::string key = match[2];
    m_gpu_properties[device][key] = m.get_value();
  }
}


void ADMetadataParser::addData(const std::vector<MetaData_t> &new_metadata){
  for(auto &m : new_metadata){
    parseMetadata(m);
  }
}

