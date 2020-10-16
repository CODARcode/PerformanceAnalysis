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
    std::unordered_map<unsigned long, unsigned long> m_idxmap; /**< Map of local function index to global function index*/
  public:

    /**
     * @brief Class constructor. 
     *
     *If a pointer to the net client is not provided the local index will not be synchronized betwee nodes
     */
    ADglobalFunctionIndexMap(ADNetClient *net_client = nullptr): m_net_client(net_client){}

    /**
     * @brief Check if the pserver is connected
     */
    bool connectedToPS() const{ return m_net_client != nullptr && m_net_client->use_ps(); }
    
    /**
     * @brief Link the net client
     */
    void linkNetClient(ADNetClient *net_client){ m_net_client = net_client; }

    /**
     * @brief Lookup the global index corresponding to the input local index
     *
     * Function names must be unique
     */
    unsigned long lookup(const unsigned long local_idx, const std::string &func_name);
    
    /**
     * @brief Lookup the global indices corresponding to the input local indices as a batch
     *
     * Function names must be unique
     */   
    std::vector<unsigned long> lookup(const std::vector<unsigned long> &local_idx, const std::vector<std::string> &func_name);


    /**
     * @brief Lookup the global index corresponding to the input local index (const version; throws if not already present)
     *
     */
    unsigned long lookup(const unsigned long local_idx) const;

    
    /**
     * @brief Return a pointer to the net client
     */
    ADNetClient* getNetClient(){ return m_net_client; }
  };

}
