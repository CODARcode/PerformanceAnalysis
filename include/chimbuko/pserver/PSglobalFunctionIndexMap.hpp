#pragma once

#include <string>
#include <iostream>
#include <unordered_map>
#include <chimbuko/net.hpp>
#include <chimbuko/util/string.hpp>

namespace chimbuko{

  /**
   * @brief A class that maintains a global mapping between function name and an index, which is to be synchronized over the nodes
   */
  class PSglobalFunctionIndexMap{
    std::unordered_map<std::string, unsigned long> m_fmap; //< The map between function name and index
    mutable std::mutex m_mutex;    
  public:
    /**
     * @brief Lookup a function by name and return the index. A new index will be assigned if the function has not been encountered before.
     */
    unsigned long lookup(const std::string &func_name){
      static unsigned long idx = 0;
      std::lock_guard<std::mutex> _(m_mutex);
      auto it = m_fmap.find(func_name);
      if(it == m_fmap.end()) return idx++;
      else return it->second;
    }
  };


  /**
   * @brief Net payload for communicating function index pserver->AD
   */
  class NetPayloadGlobalFunctionIndexMap: public NetPayloadBase{
    PSglobalFunctionIndexMap* m_idxmap;
  public:
    NetPayloadGlobalFunctionIndexMap(PSglobalFunctionIndexMap* idxmap): m_idxmap(idxmap){}
    MessageKind kind() const{ return MessageKind::FUNCTION_INDEX; }
    MessageType type() const{ return MessageType::REQ_GET; }
    void action(Message &response, const Message &message) override{
      check(message);
      if(m_idxmap == nullptr) throw std::runtime_error("Cannot retrieve function index as map object has not been linked");
      unsigned long idx = m_idxmap->lookup(message.buf()); //uses a mutex lock
      response.set_msg(anyToStr(idx), false);
    }
  };

};
