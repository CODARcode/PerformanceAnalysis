#include <chimbuko/ad/ADMetadataParser.hpp>
#include <chimbuko/util/string.hpp>
#include <regex>

using namespace chimbuko;

void ADMetadataParser::parseMetadata(const MetaData_t &m){
  std::smatch match;
  if(m.get_descr() == "CUDA Context"){
    unsigned long virtual_thread = m.get_tid();
    auto it = m_gpu_thread_map.find(virtual_thread);
    if(it == m_gpu_thread_map.end()) it = m_gpu_thread_map.emplace(virtual_thread, std::pair<int,int>({-1,-1})).first;
    it->second.second = strToAny<int>(m.get_value());
  }else if(m.get_descr() == "CUDA Device"){
    unsigned long virtual_thread = m.get_tid();
    auto it = m_gpu_thread_map.find(virtual_thread);
    if(it == m_gpu_thread_map.end()) it = m_gpu_thread_map.emplace(virtual_thread, std::pair<int,int>({-1,-1})).first;
    it->second.first = strToAny<int>(m.get_value());
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

