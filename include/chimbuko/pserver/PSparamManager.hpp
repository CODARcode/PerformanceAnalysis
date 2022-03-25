#pragma once

#include<chimbuko_config.h>
#include <chimbuko/param.hpp>

#include <shared_mutex>
#include <thread>

namespace chimbuko{
  /**
   * @brief A class that maintains multiple instances of ParamInterface (one for each worker thread)
   *
   * Each worker writes to its own instance, which are aggregated into the global model periodically
   */
  class PSparamManager{
  public:
    PSparamManager(const int nworker, const std::string &ad_algorithm);

    /**
     * @brief Set the frequency (in ms) at which the global model is updated (default 1000ms)
     *
     * Must be called before the updater thread is started
     */
    inline void setGlobalModelUpdateFrequency(const int ms){ m_agg_freq_ms = ms; }

    /**
     * @brief Start the updater thread
     */
    void startUpdaterThread();

    /**
     * @brief Stop the updater thread
     *
     * Called automatically by destructor if not called before
     */
    void stopUpdaterThread();

    /**
     * @brief Force an update of the global model
     */
    void updateGlobalModel();

    /**
     * @brief Update the parameters for the specified worker and return the current global model
     */
    std::string updateWorkerModel(const std::string &param, const int worker_id);

    /**
     * @brief Determine whether the updater thread is running
     */
    inline bool updaterThreadIsRunning() const{ return m_updater_thread != nullptr; }

    /**
     * @brief Get the number of workers
     */
    inline size_t nWorkers() const{ return m_worker_params.size(); }
    
    /**
     * @brief Get the current global model in serialized form
     */
    std::string getSerializedGlobalModel() const;
    

    ~PSparamManager();

    /**
     * @brief Set the manager to force an update of the global model every time a worker is updated
     */
    void enableForceUpdate(bool val = true){ m_force_update = val; }

  protected:
    /**
     * @brief Access the global parameter object
     *
     * This is NOT thread safe
     */
    template<typename ParamType>
    ParamType & getGlobalParams(){ return dynamic_cast<ParamType&>(*m_global_params); }

    /**
     * @brief Access the worker parameter object
     *
     * This is NOT thread safe
     */
    template<typename ParamType>
    ParamType & getWorkerParams(const int i){ return dynamic_cast<ParamType&>(*m_worker_params[i]); }

  private:
    int m_agg_freq_ms; /**< The frequence in ms at which the global model is updated. Default 1000ms*/
    ParamInterface *m_global_params; /**< The global model*/
    std::string m_latest_global_params; /**< Cache of the serialized form the the latest global model*/
    std::vector<ParamInterface*> m_worker_params; /**< The parameters for each worker*/
    std::thread* m_updater_thread; /**< The thread that updates the global model*/
    bool m_updater_exit; /**< Used to signal to the updater thread that it should exit*/
    bool m_force_update; /**< If true, the global model is updated every time a worker is updated, used for testing*/
    mutable std::shared_mutex m_mutex;
  };


  /**
   * @brief Net payload for pserver updating params from AD
   */
  class NetPayloadUpdateParamManager: public NetPayloadBase{
    PSparamManager* m_manager; /**< Pointer to the manager object*/
    int m_worker_id; /**< Worker thread index*/
    bool m_freeze; /**< If true the incoming message will be ignored and the global model returned*/
  public:
    /**
     * @brief Constructor
     * @param manager Pointer to the manager object
     * @param worker_id The index of the worker thread associated with this net payload instance
     * @param freeze If true the incoming data will be ignored and the global model returned
     */
    NetPayloadUpdateParamManager(PSparamManager* manager, int worker_id, bool freeze = false): m_manager(manager), m_worker_id(worker_id), m_freeze(freeze){}

    MessageKind kind() const override{ return MessageKind::PARAMETERS; }
    MessageType type() const override{ return MessageType::REQ_ADD; }
    void action(Message &response, const Message &message) override{
      check(message);
      response.set_msg(m_freeze ? m_manager->getSerializedGlobalModel() : m_manager->updateWorkerModel(message.buf(), m_worker_id), false);
    }
  };
  /**
   * @brief Net payload for AD updating params from pserver
   */
  class NetPayloadGetParamsFromManager: public NetPayloadBase{
    PSparamManager const* m_param;
  public:
    NetPayloadGetParamsFromManager(PSparamManager const* param): m_param(param){}
    MessageKind kind() const override{ return MessageKind::PARAMETERS; }
    MessageType type() const override{ return MessageType::REQ_GET; }
    void action(Message &response, const Message &message) override{
      check(message);
      response.set_msg(m_param->getSerializedGlobalModel(), false);
    }
  };



};
