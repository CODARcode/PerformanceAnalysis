#pragma once
#include<chimbuko_config.h>

#ifdef ENABLE_PROVDB

#include <chimbuko/ad/ADProvenanceDBengine.hpp>
#include <chimbuko/provdb/provDBclient.hpp>

namespace chimbuko{

  /**
   * @brief The type of global provenance data
   */
  enum class GlobalProvenanceDataType { FunctionStats, CounterStats };

  /**
   * @Client for the pserver interaction with the provevance database
   */
  class PSProvenanceDBclient: public provDBclient{
  private:
    std::unique_ptr<yokan::Collection> m_coll_funcstats; /**< The function statistics collection */
    std::unique_ptr<yokan::Collection> m_coll_counterstats; /**< The counter statistics collection */

    thallium::remote_procedure *m_client_hello; /**< RPC to register client with provDB */
    thallium::remote_procedure *m_client_goodbye; /**< RPC to deregister client with provDB */
    bool m_perform_handshake; /**< Optionally disable the client->server registration */

  public:
    PSProvenanceDBclient();

    ~PSProvenanceDBclient();
    
    /**
     * @brief Connect the client to the provenance database server
     * @param addr The server address
     * @param provider_idx The provider index on the server
     */
    void connect(const std::string &addr, const int provider_idx = 0);

    /**
     * @brief Connect the client the appropriate provenance database server instance using the default setup
     * @param addr_file_dir The directory containing the address files created by the provDB
     */
    void connectMultiServer(const std::string &addr_file_dir);
   
    /**
     * @brief Disconnect if presently connected
     */
    void disconnect(); 

    /**
     * @brief Enable or disable the client<->server handshake (call before connecting)
     */
    void setEnableHandshake(const bool to){ m_perform_handshake = to; }

    /**
     * @brief Get the Sonata collection associated with the data type allowing access to more sophisticated functionality
     */
    yokan::Collection & getCollection(const GlobalProvenanceDataType type);
    const yokan::Collection & getCollection(const GlobalProvenanceDataType type) const;
		
    /**
     * @brief Send data JSON objects synchronously to the database (blocking)
     * @param entry JSON data
     * @param type The data type
     * @return Entry index
     */
    yk_id_t sendData(const nlohmann::json &entry, const GlobalProvenanceDataType type) const;

    /**
     * @brief Send *JSON array* of JSON objects synchronously to the database (blocking). This is intended for sending many independent data entries at once.
     * @param entries JSON array of data
     * @param type The data type
     * @return Vector of entry indices. Array will have zero size if database not connected.
     */
    std::vector<yk_id_t> sendMultipleData(const nlohmann::json &entries, const GlobalProvenanceDataType type) const;

    /**
     * @brief Retrieve an inserted JSON object synchronously from the database by index (blocking) (primarily for testing)
     * @param[out] entry The JSON object (if valid)
     * @param[in] index The entry index
     * @param[in] type The data type
     * @return True if the client was able to retrieve the object
     */
    bool retrieveData(nlohmann::json &entry, yk_id_t index, const GlobalProvenanceDataType type) const;

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
    void linkPerf(PerfStats *stats);
  };

};

#endif
