#pragma once
#include <chimbuko_config.h>
#include <mutex>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <chimbuko/util/RunStats.hpp>
#include <chimbuko/core/net.hpp>

namespace chimbuko {

  /**
   * @brief The general interface for storing function statistics for anomaly detection
   */
  class ParamInterface {
  public:
    ParamInterface();

    virtual ~ParamInterface(){};

    /**
     * @brief Return a pointer to a new instance of the ParamInterface derived class associated with the given algorithm
     */
    static ParamInterface *set_AdParam(const std::string & ad_algorithm);

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
     * @param return_update Indicates that the function should return a serialized copy of the updated parameters
     * @return An empty string or a serialized copy of the updated parameters depending on return_update
     *
     * Note: we combine update and serialize here in order to avoid having to acquire 2 successive mutex locks on the pserver
     */
    virtual std::string update(const std::string& parameters, bool return_update=false) = 0;

    
    /**
     * @brief Update the internal run statistics with those from another instance
     *
     * The instance will be dynamically cast to the derived type internally, and will throw an error if the types do not match
     * The other instance will be locked during the process for thread safety
     */
    virtual void update(const ParamInterface &other) = 0;

    /**
     * @brief Update the internal run statistics with those from multiple other instances
     *
     * The instance will be dynamically cast to the derived type internally, and will throw an error if the types do not match
     * The other instance will be locked during the process for thread safety
     */
    virtual void update(const std::vector<ParamInterface*> &other);

    /**
     * @brief Set the internal run statistics to match those included in the serialized input map. Overwrite performed only for those keys in input.
     * @param runstats The serialized input map
     */
    virtual void assign(const std::string& parameters) = 0;

    virtual void show(std::ostream& os) const = 0;

    /**
     * @brief Get the algorithm parameters associated with a given function. Format is algorithm dependent
     */
    virtual nlohmann::json get_algorithm_params(const unsigned long func_id) const = 0;

    /**
     * @brief Get the algorithm parameters for all functions. 
     * @param func_id_map A map of function index -> (program index, function name) used to populate fields in the output
     */
    virtual nlohmann::json get_algorithm_params(const std::unordered_map<unsigned long, std::pair<unsigned long, std::string> > & func_id_map) const = 0;

    /**
     * @brief Get the algorithm parameters in JSON form
     */
    virtual nlohmann::json get_json() const = 0;

    /**
     * @brief Set the algorithm parameters from the input JSON structure
     */
    virtual void set_json(const nlohmann::json &from) = 0;

  protected:
    mutable std::mutex m_mutex; // used to update parameters
  };


  /**
   * @brief Net payload for pserver updating params from AD
   */
  class NetPayloadUpdateParams: public NetPayloadBase{
    ParamInterface * m_param;
    bool m_freeze; /**< If set to true, the additional data from the AD will be ignored and the parameter state will not change*/
  public:
    /**
     * @brief Construct the NetPayloadUpdateParams object
     * @param param A pointer to an instance of ParamInterface
     * @param freeze If true the state will not be modified by the update command
     */
    NetPayloadUpdateParams(ParamInterface *param, bool freeze = false): m_param(param), m_freeze(freeze){}
    MessageKind kind() const override{ return MessageKind::PARAMETERS; }
    MessageType type() const override{ return MessageType::REQ_ADD; }
    void action(Message &response, const Message &message) override{
      check(message);
      //Response message is a copy of the updated statistics in JSON form
      response.set_msg(m_freeze ?
		       m_param->serialize() :
		       m_param->update(message.buf(), true),  //second argument indicates that a serialized copy of the updated params should be returned
		       false);
    }
  };
  /**
   * @brief Net payload for AD updating params from pserver
   */
  class NetPayloadGetParams: public NetPayloadBase{
    ParamInterface const* m_param;
  public:
    NetPayloadGetParams(ParamInterface const* param): m_param(param){}
    MessageKind kind() const override{ return MessageKind::PARAMETERS; }
    MessageType type() const override{ return MessageType::REQ_GET; }
    void action(Message &response, const Message &message) override{
      check(message);
      response.set_msg(m_param->serialize(), false);
    }
  };



} // end of chimbuko namespace
