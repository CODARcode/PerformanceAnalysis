#pragma once
#include "chimbuko/global_anomaly_stats.hpp"
#include "chimbuko/util/threadPool.hpp"
#include "chimbuko/param.hpp"
#include "chimbuko/message.hpp"
#include <string>
#include <thread>

namespace chimbuko {

  /**
   * @brief enum network thread level (for MPI)
   * 
   */
  enum class NetThreadLevel {
    THREAD_MULTIPLE = 3
  };


  class NetPayloadBase{
  public:
    /**
     * @brief The message kind to which the payload is to be bound
     */
    virtual MessageKind kind() const = 0;

    /**
     * @brief The message type to which the payload is to be bound
     */
    virtual MessageType type() const = 0;
    
    /**
     * @brief Act on the message and formulate a response
     */
    virtual void action(Message &response, const Message &message) = 0;

    /**
     * @brief Helper function to ensure the message is of the correct kind/type
     */
    void check(const Message &msg) const{
      if(msg.kind() != kind()) throw std::runtime_error("Message has the wrong kind");
      if(msg.type() != type()) throw std::runtime_error("Message has the wrong kind");
    }

    virtual ~NetPayloadBase(){}
  };
  /**
   * @brief Default handshake response; this is bound automatically to the network
   */
  class NetPayloadHandShake: public NetPayloadBase{
  public:
    MessageKind kind() const{ return MessageKind::DEFAULT; }
    MessageType type() const{ return MessageType::REQ_ECHO; }
    void action(Message &response, const Message &message) override{
      check(message);
      response.set_msg(std::string("Hello!I am NET!"), false);
    };
  };
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
  class NetPayloadUpdateAnomalyStats: public NetPayloadBase{
    GlobalAnomalyStats * m_global_anom_stats;
  public:
    NetPayloadUpdateAnomalyStats(GlobalAnomalyStats * global_anom_stats): m_global_anom_stats(global_anom_stats){}
    MessageKind kind() const{ return MessageKind::ANOMALY_STATS; }
    MessageType type() const{ return MessageType::REQ_ADD; }
    void action(Message &response, const Message &message) override{
      check(message);
      if(m_global_anom_stats == nullptr) throw std::runtime_error("Cannot update global anomaly statistics as stats object has not been linked");
      m_global_anom_stats->add_anomaly_data(message.buf());
      response.set_msg("", false);
    }
  };


  /**
   * @brief Network interface class
   * 
   */
  class NetInterface {
  public:
    /**
     * @brief Construct a new Net Interface object
     * 
     */
    NetInterface();

    /**
     * @brief Destroy the Net Interface object
     * 
     */
    virtual ~NetInterface();

    /**
     * @brief (virtual) initialize network interface
     * 
     * @param argc command line argc 
     * @param argv command line argv
     * @param nt the number of threads for a thread pool
     */
    virtual void init(int* argc = nullptr, char*** argv = nullptr, int nt=1) = 0;

    /**
     * @brief (virtual) finalize network
     * 
     */
    virtual void finalize() = 0;

    /**
     * @brief (virtual) run network server
     * 
     */
#ifdef _PERF_METRIC
    virtual void run(std::string logdir="./") = 0;
#else
    virtual void run() = 0;
#endif
    /**
     * @brief (virtual) stop network server
     * 
     */
    virtual void stop() = 0;

    /**
     * @brief (virtual) name of network server
     * 
     * @return std::string name of network server
     */
    virtual std::string name() const = 0;


    /**
     * @brief Add a payload to the receiver bound to particular message kind/type specified internally
     *
     * Assumes ownership of the NetPayloadBase object and deletes in constructor
     */
    void add_payload(NetPayloadBase* payload);

  protected:
    /**
     * @brief initialize thread pool 
     * 
     * @param nt the number threads in the pool
     */
    virtual void init_thread_pool(int nt) = 0;

  protected:
    int              m_nt;    /**< The number of threads in the pool */
    
    std::unordered_map<MessageKind,
		       std::unordered_map<MessageType,  std::unique_ptr<NetPayloadBase> >
		       > m_payloads;
  };

  namespace DefaultNetInterface
  {
    /**
     * @brief get default network interface for easy usages
     * 
     * @return NetInterface& default network 
     */
    NetInterface& get();
  }

} // end of chimbuko namespace
