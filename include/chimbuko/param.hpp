#pragma once
#include <mutex>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <chimbuko/util/RunStats.hpp>
#include <chimbuko/net.hpp>

namespace chimbuko {

  /** 
   * @brief The general interface for storing function statistics for anomaly detection
   */      
  class ParamInterface {
  public:
    ParamInterface();

    virtual ~ParamInterface(){};

    /**
     * @brief Clear all statistics
     */
    virtual void clear() = 0;

    /**
     * @brief Get the number of functions for which statistics are being collected
    */
    virtual size_t size() const = 0;

    /**
     * @brief Convert internal run statistics to string format for IO
     * @return Run statistics in string format
     */
    virtual std::string serialize() const = 0;

    /**
     * @brief Update the internal run statistics with those included in the serialized input map
     * @param parameters The parameters in serialized format
     * @param flag The meaning of the flag is dependent on the implementation
     * @return Returned contents dependent on implementation
     */
    virtual std::string update(const std::string& parameters, bool flag=false) = 0;

    /**
     * @brief Set the internal run statistics to match those included in the serialized input map. Overwrite performed only for those keys in input.
     * @param runstats The serialized input map
     */
    virtual void assign(const std::string& parameters) = 0;

    virtual void show(std::ostream& os) const = 0;

    /**
     * @brief Get the statistics associated with a given function
     */
    virtual const RunStats & get_function_stats(const unsigned long func_id) const = 0;

  protected:
    mutable std::mutex m_mutex; // used to update parameters
  };


  /** 
   * @brief Net payload for pserver updating params from AD
   */
  class NetPayloadUpdateParams: public NetPayloadBase{
    ParamInterface * m_param;
  public:
    NetPayloadUpdateParams(ParamInterface *param): m_param(param){}
    MessageKind kind() const{ return MessageKind::PARAMETERS; }
    MessageType type() const{ return MessageType::REQ_ADD; }
    void action(Message &response, const Message &message) override{
      check(message);
      response.set_msg(m_param->update(message.buf(), true), false);
    }	
  };
  /** 
   * @brief Net payload for AD updating params from pserver
   */
  class NetPayloadGetParams: public NetPayloadBase{
    ParamInterface const* m_param;
  public:
    NetPayloadGetParams(ParamInterface const* param): m_param(param){}
    MessageKind kind() const{ return MessageKind::PARAMETERS; }
    MessageType type() const{ return MessageType::REQ_GET; }
    void action(Message &response, const Message &message) override{
      check(message);
      response.set_msg(m_param->serialize(), false);
    }	
  };



} // end of chimbuko namespace
