#include <chimbuko/ad/ADMetadataParser.hpp>
#include <chimbuko/core/util/string.hpp>
#include <regex>
#include <chimbuko/verbose.hpp>

using namespace chimbuko;

GPUvirtualThreadInfo & getGPUthreadInfoStruct(const unsigned long virtual_thread, std::unordered_map<unsigned long, GPUvirtualThreadInfo> &map){
  auto it = map.find(virtual_thread);
  if(it == map.end()) 
    it = map.emplace(virtual_thread, GPUvirtualThreadInfo(virtual_thread,-1,-1,0) ).first;
  return it->second;
}


void ADMetadataParser::parseMetadata(const MetaData_t &m){
  std::smatch match;
  if(m.get_descr() == "Hostname"){
    m_hostname = m.get_value(); //should be available on any linux machine
  }else if(m.get_descr() == "CUDA Context"){
    unsigned long virtual_thread = m.get_tid();
    getGPUthreadInfoStruct(m.get_tid(), m_gpu_thread_map).context = strToAny<unsigned long>(m.get_value());
    verboseStream << "ADMetadataParser mapped thread " << m.get_tid() << " to CUDA context " << getGPUthreadInfoStruct(m.get_tid(), m_gpu_thread_map).context << std::endl;
  }else if(m.get_descr() == "CUDA Stream"){
    getGPUthreadInfoStruct(m.get_tid(), m_gpu_thread_map).stream = strToAny<unsigned long>(m.get_value());
    verboseStream << "ADMetadataParser mapped thread " << m.get_tid() << " to CUDA stream " << getGPUthreadInfoStruct(m.get_tid(), m_gpu_thread_map).stream << std::endl;
  }else if(m.get_descr() == "CUDA Device"){
    getGPUthreadInfoStruct(m.get_tid(), m_gpu_thread_map).device = strToAny<unsigned long>(m.get_value());
    verboseStream << "ADMetadataParser mapped thread " << m.get_tid() << " to CUDA device " << getGPUthreadInfoStruct(m.get_tid(), m_gpu_thread_map).device << std::endl;
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

const GPUvirtualThreadInfo & ADMetadataParser::getGPUthreadInfo(const unsigned long thread) const{
  auto it = m_gpu_thread_map.find(thread);
  if(it == m_gpu_thread_map.end()) throw std::runtime_error("Thread is not associated with a GPU device/context/stream");
  return it->second;
}
