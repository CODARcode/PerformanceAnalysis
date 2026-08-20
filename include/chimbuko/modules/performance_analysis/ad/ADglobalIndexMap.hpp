#pragma once
#include <chimbuko_config.h>
#include <cassert>
#include <unordered_map>
#include <chimbuko/core/ad/ADNetClient.hpp>
#include <chimbuko/core/util/string.hpp>
#include <chimbuko/core/verbose.hpp>
#include <chimbuko/modules/performance_analysis/pserver/PScommon.hpp>

namespace chimbuko{
  namespace modules{
    namespace performance_analysis{


      /**
       * @brief A class that maintains a mapping of a local index to a global index that is specified by the parameter server
       *
       * If the parameter server is not connected it will simply return the local index
       */
      class ADglobalIndexMap{
	ADNetClient *m_net_client;
	std::unordered_map<unsigned long, unsigned long> m_idxmap; /**< Map of local index to global index*/	
	unsigned long m_pid; /**< Program index*/
	MessageKind m_msg_kind; //*< The RPC "kind" used to identify this index on the server */
      public:

	/**
	 * @brief Class constructor. 
	 * @param pid The program index
	 * @param msg_kind The RPC "kind" used to identify this index on the server
	 * @param A pointer to the ADNetClient
	 *
	 *If a pointer to the net client is not provided the local index will not be synchronized betwee nodes
	 */
	ADglobalIndexMap(unsigned long pid, MessageKind msg_kind, ADNetClient *net_client = nullptr): m_net_client(net_client), m_pid(pid), m_msg_kind(msg_kind){}

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
	 * Names must be unique and universal
	 */
	unsigned long lookup(const unsigned long local_idx, const std::string &name);
    
	/**
	 * @brief Lookup the global indices corresponding to the input local indices as a batch
	 *
	 * Names must be unique and universal
	 */   
	std::vector<unsigned long> lookup(const std::vector<unsigned long> &local_idx, const std::vector<std::string> &name);


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


      /**
       * @brief A class that maintains a mapping of a string to a global index that is specified by the parameter server
       *
       * If the parameter server is not connected it will assign indices internally
       */
      class ADglobalStringIndexMap{
	ADNetClient *m_net_client;	
	unsigned long m_pid; /**< Program index*/
	std::unordered_map<std::string, unsigned long> m_idxmap; /**< Map of local name to global index*/	
	MessageKind m_msg_kind; //*< The RPC "kind" used to identify this index on the server */
	unsigned long m_disconnected_globidx; //*< A counter used to assign a unique index to a string in the case where the pserver is not connected
      public:

	/**
	 * @brief Class constructor. 
	 * @param pid The program index
	 * @param msg_kind The RPC "kind" used to identify this index on the server
	 * @param A pointer to the ADNetClient
	 *
	 *If a pointer to the net client is not provided the local index will not be synchronized betwee nodes
	 */
	ADglobalStringIndexMap(unsigned long pid, MessageKind msg_kind, ADNetClient *net_client = nullptr): m_net_client(net_client), m_pid(pid), m_msg_kind(msg_kind), m_disconnected_globidx(0){}

	/**
	 * @brief Check if the pserver is connected
	 */
	bool connectedToPS() const{ return m_net_client != nullptr && m_net_client->use_ps(); }
    
	/**
	 * @brief Link the net client
	 */
	void linkNetClient(ADNetClient *net_client);

	/**
	 * @brief Lookup the global index corresponding to the string name
	 *
	 * Names must be unique and universal
	 */
	unsigned long lookup(const std::string &name);
    
	/**
	 * @brief Lookup the global indices corresponding to the string name as a batch
	 *
	 * Names must be unique and universal
	 */   
	std::vector<unsigned long> lookup(const std::vector<std::string> &names);
    
	/**
	 * @brief Return a pointer to the net client
	 */
	ADNetClient* getNetClient(){ return m_net_client; }
      };



    }
  }
}
