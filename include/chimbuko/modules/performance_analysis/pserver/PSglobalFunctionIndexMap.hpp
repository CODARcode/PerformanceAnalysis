#pragma once
#include <chimbuko_config.h>
#include <chimbuko/modules/performance_analysis/pserver/PScommon.hpp>
#include <string>
#include <iostream>
#include <unordered_map>
#include <chimbuko/core/net.hpp>
#include <chimbuko/core/util/string.hpp>
#include <nlohmann/json.hpp>

namespace chimbuko{

  /**
   * @brief A class that maintains a global mapping between function name and an index, which is to be synchronized over the nodes
   */
  class PSglobalFunctionIndexMap{
    std::unordered_map<unsigned long, std::unordered_map<std::string, unsigned long> > m_fmap; /**< The map between the program index and function name to the unique global index: [pid][func_name] -> fid */
    mutable std::mutex m_mutex;    
    unsigned long m_idx; /** < Next unassigned index */
  public:
    PSglobalFunctionIndexMap(): m_idx(0){}

    /**
     * @brief Lookup a function by name and return the index. A new index will be assigned if the function has not been encountered before.
     * @param pid The program index
     * @param func_name The function name
     */
    unsigned long lookup(unsigned long pid, const std::string &func_name);
    
    /**
     * @brief Check if the map contains the specified function 
     * @param pid The program index
     * @param func_name The function name
     */
    bool contains(unsigned long pid, const std::string &func_name) const;

    /**
     * @brief Get a map between the function index and the pid /function name
     * @return A map of function index -> (program index, function name)
     */
    std::unordered_map<unsigned long, std::pair<unsigned long, std::string> > getFunctionIndexMap() const;

    /**
     * @brief Serialize the map to a JSON object
     */
    nlohmann::json serialize() const;

    /**
     * @brief Set the map to the contents of the JSON object
     */
    void deserialize(const nlohmann::json &fmap);
  };


  /**
   * @brief Net payload for communicating function index pserver->AD
   */
  class NetPayloadGlobalFunctionIndexMap: public NetPayloadBase{
    PSglobalFunctionIndexMap* m_idxmap;
  public:
    NetPayloadGlobalFunctionIndexMap(PSglobalFunctionIndexMap* idxmap): m_idxmap(idxmap){}
    int kind() const override{ return MessageKind::FUNCTION_INDEX; }
    MessageType type() const override{ return MessageType::REQ_GET; }
    void action(Message &response, const Message &message) override;
  };

  /**
   * @brief Net payload for communicating function index pserver->AD in batches
   */
  class NetPayloadGlobalFunctionIndexMapBatched: public NetPayloadBase{
    PSglobalFunctionIndexMap* m_idxmap;
  public:
    NetPayloadGlobalFunctionIndexMapBatched(PSglobalFunctionIndexMap* idxmap): m_idxmap(idxmap){}
    int kind() const override{ return MessageKind::FUNCTION_INDEX; }
    MessageType type() const override{ return MessageType::REQ_GET; }
    void action(Message &response, const Message &message) override;
  };



};
