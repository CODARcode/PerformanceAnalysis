#pragma once
#include <chimbuko_config.h>
#include "chimbuko/util/threadPool.hpp"
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

#ifdef _PERF_METRIC
    /**
     * @brief (virtual) Run network server with performance logging to provided directory
     */
    virtual void run(std::string logdir="./") = 0;
#else
    /**
     * @brief (virtual) Run network server
     */    
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
     * @param payload The payload
     * @param worker_idx The worker index to which the payload is bound (implementation defined, see below)
     *
     * Assumes ownership of the NetPayloadBase object and deletes in constructor
     * worker_idx:
     *     ZMQNet - use 0 always
     *     MPINet - use 0 always
     *     ZMQMENet - worker_idx corresponds to the endpoint thread
     */
    void add_payload(NetPayloadBase* payload, int worker_idx = 0);

    /**
     * @brief Print information on the payloads
     */
    void list_payloads(std::ostream &os) const;

    typedef std::unordered_map<MessageKind, std::unordered_map<MessageType,  std::unique_ptr<NetPayloadBase> > > PayloadMapType; /**< Map of message kind/type to payloads */
    typedef std::unordered_map<int, PayloadMapType> WorkerPayloadMapType; /**< Map of worker index and message type to payloads */

    /**
     * @brief Find the action associated with the given worker_id and message type and perform the action
     * @param worker_id The worker index
     * @param msg_reply The reply message
     * @param msg The input message
     * @param payloads The map of worker/message type to payload
     */
    static void find_and_perform_action(int worker_id, Message &msg_reply, const Message &msg, const WorkerPayloadMapType &payloads);

    /**
     * @brief Find the action associated with the given message type and perform the action
     * @param msg_reply The reply message
     * @param msg The input message
     * @param payloads The map of worker/message type to payload
     */
    static void find_and_perform_action(Message &msg_reply, const Message &msg, const PayloadMapType &payloads);

  protected:
    /**
     * @brief initialize thread pool 
     * 
     * @param nt the number threads in the pool
     */
    virtual void init_thread_pool(int nt) = 0;

    int              m_nt;    /**< The number of threads in the pool */

    WorkerPayloadMapType m_payloads; /**< Map of worker index (implementation defined), message kind and message type to a payload*/
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
