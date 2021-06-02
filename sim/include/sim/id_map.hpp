#pragma once

#include<string>
#include<unordered_map>

namespace chimbuko_sim{
  //All functions must be registered
  std::unordered_map<std::string, unsigned long> & funcIdxMap();

  //Automatically register counter names with indices
  class CounterIdxManager{
    std::unordered_map<std::string, unsigned long> cid_map;
    std::unordered_map<int, std::string> cname_map;

  public:  
    /**
     * @brief Get an index for the counter 'cname'. A new index will be generated the first time a counter is seen
     */
    long getIndex(const std::string &cname);
  
    const std::unordered_map<int, std::string>* getCounterMap(){ return &cname_map; }
  };

  inline CounterIdxManager & getCidxManager(){ static CounterIdxManager cman; return cman; }
};
