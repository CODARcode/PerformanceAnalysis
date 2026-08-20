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
  namespace modules{
    namespace performance_analysis{

      /**
       * @brief A class that maintains a global mapping between pid+name and an index, which is to be synchronized over the nodes
       */
      class PSglobalIndexMap{
	std::unordered_map<unsigned long, std::unordered_map<std::string, unsigned long> > m_fmap; /**< The map between the program index and name to the unique global index: [pid][func_name] -> fid */
	mutable std::mutex m_mutex;    
	unsigned long m_idx; /** < Next unassigned index */
      public:
	PSglobalIndexMap(): m_idx(0){}

	/**
	 * @brief Lookup by name and return the index. A new index will be assigned if the name has not been encountered before.
	 * @param pid The program index
	 * @param name The name
	 */
	unsigned long lookup(unsigned long pid, const std::string &name);
    
	/**
	 * @brief Check if the map contains the specified name
	 * @param pid The program index
	 * @param name The name
	 */
	bool contains(unsigned long pid, const std::string &name) const;

	/**
	 * @brief Get a map between the index and the pid+name
	 * @return A map of index -> (program index, name)
	 */
	std::unordered_map<unsigned long, std::pair<unsigned long, std::string> > getIndexMap() const;

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
      class NetPayloadGlobalIndexMap: public NetPayloadBase{
	PSglobalIndexMap* m_idxmap;
	MessageKind m_kind;
      public:
	NetPayloadGlobalIndexMap(PSglobalIndexMap* idxmap, MessageKind kind): m_idxmap(idxmap), m_kind(kind){}
	int kind() const override{ return m_kind; }
	MessageType type() const override{ return MessageType::REQ_GET; }
	void action(Message &response, const Message &message) override;
      };

      /**
       * @brief Net payload for communicating function index pserver->AD in batches
       */
      class NetPayloadGlobalIndexMapBatched: public NetPayloadBase{
	PSglobalIndexMap* m_idxmap;
		MessageKind m_kind;
      public:
	NetPayloadGlobalIndexMapBatched(PSglobalIndexMap* idxmap, MessageKind kind): m_idxmap(idxmap), m_kind(kind){}
	int kind() const override{ return m_kind; }
	MessageType type() const override{ return MessageType::REQ_GET; }
	void action(Message &response, const Message &message) override;
      };



    };
  }
}
