#pragma once
#include<chimbuko_config.h>
#include<chimbuko/ad/ExecData.hpp>
#include<unordered_map>

namespace chimbuko{

  /**
   * @brief Structure containing the CUDA device/context/stream associated with a given virtual thread index
   */
  struct GPUvirtualThreadInfo{
    unsigned long thread; /**<The virtual thread index assigned by Tau*/
    unsigned long device; /**<The device index (assigned by the CUDA runtime)*/
    unsigned long context; /**<The device context*/
    unsigned long stream; /**<Stream index if multiple streams are in use. Defaults to 0 if only one stream*/

    GPUvirtualThreadInfo(unsigned long _thread, unsigned long _device, unsigned long _context, unsigned long _stream = 0): thread(_thread), device(_device), context(_context), stream(_stream){}
    
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
    std::string m_hostname; /**< The host name*/

    /**
     * @brief Parse an individual metadata entry
     */
    void parseMetadata(const MetaData_t &m);
    
  public:
    ADMetadataParser(): m_hostname("Unknown"){}
    
    /**
     * @brief Add new metadata collected during this timeframe
     */
    void addData(const std::vector<MetaData_t> &new_metadata);

    const std::unordered_map<unsigned long, GPUvirtualThreadInfo> &getGPUthreadMap() const{ return m_gpu_thread_map; }
    bool isGPUthread(const unsigned long thr) const{ return m_gpu_thread_map.count(thr) != 0; }
    
    /**
     * @brief Return the thread info struct for this thread. Throws an error if an invalid thread
     */
    const GPUvirtualThreadInfo & getGPUthreadInfo(const unsigned long thread) const;

    
    /**
     * @brief Get the map of CUDA device index to a key/value pair of GPU properties
     */
    const std::unordered_map<int, std::unordered_map<std::string, std::string> > & getGPUproperties() const{ return m_gpu_properties; }

    const std::string & getHostname() const{ return m_hostname; }
  };



}
