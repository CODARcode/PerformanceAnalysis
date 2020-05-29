#pragma once

#include<config.h>
#include<chimbuko/ad/ExecData.hpp>
#include<unordered_map>

namespace chimbuko{

  /**
   * @brief A class that parses and maintains useful metadata
   */
  class ADMetadataParser{
    std::unordered_map<unsigned long, std::pair<int,int> > m_gpu_thread_map; /**< Map of tau's virtual thread index to a CUDA device and context*/
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

    const std::unordered_map<unsigned long, std::pair<int,int> > &getGPUthreadMap() const{ return m_gpu_thread_map; }
    bool isGPUthread(const unsigned long thr) const{ return m_gpu_thread_map.count(thr) != 0; }

    const std::unordered_map<int, std::unordered_map<std::string, std::string> > & getGPUproperties() const{ return m_gpu_properties; }
  };



}
