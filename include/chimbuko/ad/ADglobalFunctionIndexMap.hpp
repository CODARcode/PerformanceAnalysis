#pragma once

#include <cassert>
#include <unordered_map>
#include "ADNetClient.hpp"
#include <chimbuko/util/string.hpp>
#include <chimbuko/verbose.hpp>

namespace chimbuko{

  /**
   * @brief A class that maintains a mapping of a local function index to a global function index that is specified by the parameter server
   *
   * If the parameter server is not connected it will simply return the local index
   */
  class ADglobalFunctionIndexMap{
    ADNetClient *m_net_client;
    std::unordered_map<unsigned long, unsigned long> m_idxmap;
  public:

    ADglobalFunctionIndexMap(ADNetClient *net_client = nullptr): m_net_client(net_client){}
    
    /**
     * @brief Lookup the global index corresponding to the input local index
     *
     * Function names must be unique
     */
    unsigned long lookup(const unsigned long local_idx, const std::string &func_name);
  };

}
