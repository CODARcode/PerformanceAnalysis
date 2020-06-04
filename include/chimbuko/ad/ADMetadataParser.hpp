#pragma once

#include<config.h>
#include<chimbuko/ad/ExecData.hpp>
#include<unordered_map>

namespace chimbuko{

  /**
   * @brief Structure containing the CUDA device/context/stream associated with a given virtual thread index
   */
  struct GPUvirtualThreadInfo{
    unsigned long thread; /**<The virtual thread index assigned by Tau*/
    int device; /**<The device index (assigned by the CUDA runtime)*/
    int context; /**<The device context*/
    int stream; /**<Stream index if multiple streams are in use. Defaults to 0 if only one stream*/

    GPUvirtualThreadInfo(unsigned long _thread, int _device, int _context, int _stream = 0): thread(_thread), device(_device), context(_context), stream(_stream){}
    
    /**
     * @brief Get the data as a JSON object
     */
    nlohmann::json get_json() const{ return { {"thread", thread}, {"device", device}, {"context", context}, {"stream", stream } }; };
  };

  /**
   * @brief A class that parses and maintains useful metadata
   */
  class ADMetadataParser{
    std::unordered_map<unsigned long, GPUvirtualThreadInfo> m_gpu_thread_map; /**< Map of tau's virtual thread index to CUDA device/context/stream*/
    std::unordered_map<int, std::unordered_map<std::string, std::string> > m_gpu_properties; /**< Properties of GPU device. Index is the CUDA device index */

    /**
     * @brief Parse an individual metadata entry
     */
    void parseMetadata(const MetaData_t &m);
    
  public:
    /**
     * @brief Add new metadata collected during this timeframe
     */
    void addData(const std::vector<MetaData_t> &new_metadata);

    const std::unordered_map<unsigned long, GPUvirtualThreadInfo> &getGPUthreadMap() const{ return m_gpu_thread_map; }
    bool isGPUthread(const unsigned long thr) const{ return m_gpu_thread_map.count(thr) != 0; }

    const std::unordered_map<int, std::unordered_map<std::string, std::string> > & getGPUproperties() const{ return m_gpu_properties; }
  };



}
