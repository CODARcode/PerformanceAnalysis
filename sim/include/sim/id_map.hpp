#pragma once
#include <chimbuko_config.h>
#include<string>
#include<unordered_map>

namespace chimbuko_sim{
  /**
   * @brief Map of function name to index
   */
  std::unordered_map<std::string, unsigned long> & funcIdxMap();

  /**
   * @brief Register a function (set up the function idx map)
   * @param func_name The function name
   * @return The assigned function index
   */
  unsigned long registerFunc(const std::string &func_name);


  /**
   * @brief A class to automatically register counter names with indices
   */
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

  /**
   * @brief Get the global shared instance of the counter index manager
   */
  inline CounterIdxManager & getCidxManager(){ static CounterIdxManager cman; return cman; }
};
