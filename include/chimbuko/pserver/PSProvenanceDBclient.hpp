#pragma once
#include<chimbuko_config.h>

#ifdef ENABLE_PROVDB

#include <sonata/Client.hpp>
#include <nlohmann/json.hpp>
#include <queue>
#include <chimbuko/ad/ADProvenanceDBengine.hpp>
#include <chimbuko/util/PerfStats.hpp>

namespace chimbuko{

  /**
   * @brief The type of global provenance data
   */
  enum class GlobalProvenanceDataType { FunctionStats, CounterStats };

  /**
   * @Client for the pserver interaction with the provevance database
   */
  class PSProvenanceDBclient{
  private:
    sonata::Client m_client; /**< Sonata client */
    sonata::Database m_database; /**< Sonata database */
    sonata::Collection m_coll_funcstats; /**< The function statistics collection */
    sonata::Collection m_coll_counterstats; /**< The counter statistics collection */
    bool m_is_connected; /**< True if connection has been established to the provider */
    
    thallium::endpoint m_server; /**< Endpoint for provDB comms*/
    thallium::remote_procedure *m_client_hello; /**< RPC to register client with provDB */
    thallium::remote_procedure *m_client_goodbye; /**< RPC to deregister client with provDB */

    PerfStats *m_stats; /**< Performance data gathering*/

  public:
    PSProvenanceDBclient(): m_is_connected(false), m_client_hello(nullptr), m_client_goodbye(nullptr), m_stats(nullptr){}

    ~PSProvenanceDBclient();
    
    /**
     * @brief Connect the client to the provenance database server
     * @param addr The server address
     */
    void connect(const std::string &addr);

    /**
     * @brief Check if connnection has been established to provider
     */
    bool isConnected() const{ return m_is_connected; }
    
    /**
     * @brief Disconnect if presently connected
     */
    void disconnect(); 


    /**
     * @brief Get the Sonata collection associated with the data type allowing access to more sophisticated functionality
     */
    sonata::Collection & getCollection(const GlobalProvenanceDataType type);
    const sonata::Collection & getCollection(const GlobalProvenanceDataType type) const;
		
    /**
     * @brief Send data JSON objects synchronously to the database (blocking)
     * @param entry JSON data
     * @param type The data type
     * @return Entry index
     */
    uint64_t sendData(const nlohmann::json &entry, const GlobalProvenanceDataType type) const;

    /**
     * @brief Send *JSON array* of JSON objects synchronously to the database (blocking). This is intended for sending many independent data entries at once.
     * @param entries JSON array of data
     * @param type The data type
     * @return Vector of entry indices. Array will have zero size if database not connected.
     */
    std::vector<uint64_t> sendMultipleData(const nlohmann::json &entries, const GlobalProvenanceDataType type) const;

    /**
     * @brief Retrieve an inserted JSON object synchronously from the database by index (blocking) (primarily for testing)
     * @param[out] entry The JSON object (if valid)
     * @param[in] index The entry index
     * @param[in] type The data type
     * @return True if the client was able to retrieve the object
     */
    bool retrieveData(nlohmann::json &entry, uint64_t index, const GlobalProvenanceDataType type) const;

    /**
     * @brief Retrieve all records from the database
     */
    std::vector<std::string> retrieveAllData(const GlobalProvenanceDataType type) const;

    /**
     * @brief Apply a jx9 filter to the database and retrieve all records that match
     */
    std::vector<std::string> filterData(const GlobalProvenanceDataType type, const std::string &query) const;

    /**
     * @brief Link a PerfStats instance to monitor performance
     */
    void linkPerf(PerfStats *stats){ m_stats = stats; }
  };

};

#endif
