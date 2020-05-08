#pragma once
#include<config.h>

#ifdef ENABLE_PROVDB

#include <sonata/Client.hpp>
#include <nlohmann/json.hpp>

namespace chimbuko{

  enum class ProvenanceDataType { AnomalyData, Metadata };

  /**
   * @Client for interacting with provevance database
   */
  class ADProvenanceDBclient{
  private:
    static thallium::engine m_engine; /**< Thallium engine */
    sonata::Client m_client; /**< Sonata client */
    sonata::Database m_database; /**< Sonata database */
    sonata::Collection m_coll_anomalies; /**< The anomalies collection */
    sonata::Collection m_coll_metadata; /**< The metadata collection */
    bool m_is_connected; /**< True if connection has been established to the provider */
  public:
    ADProvenanceDBclient(): m_client(m_engine), m_is_connected(false){}
    
    /**
     * @brief Connect the client to the provenance database server
     * @param addr The server address
     */
    void connect(const std::string &addr);

    /**
     * @brief Check if connnection has been established to provider
     */
    bool isConnected() const{ return m_is_connected; }
    

    ~ADProvenanceDBclient();

    /**
     * @brief Get the Sonata collection associated with the data type allowing access to more sophisticated functionality
     */
    inline sonata::Collection & getCollection(const ProvenanceDataType type){ return type == ProvenanceDataType::AnomalyData ? m_coll_anomalies : m_coll_metadata; }
    inline const sonata::Collection & getCollection(const ProvenanceDataType type) const{ return type == ProvenanceDataType::AnomalyData ? m_coll_anomalies : m_coll_metadata; }
		
    /**
     * @brief Send data JSON objects to the database
     * @param JSON data
     * @param type The data type
     * @return Entry index
     */
    uint64_t sendData(const nlohmann::json &entry, const ProvenanceDataType type) const;

    /**
     * @brief Retrieve an inserted JSON object from the database by index (primarily for testing)
     * @param[out] entry The JSON object (if valid)
     * @param[in] index The entry index
     * @param[in] type The data type
     * @return True if the client was able to retrieve the object
     */
    bool retrieveData(nlohmann::json &entry, uint64_t index, const ProvenanceDataType type) const;
  };

};

#endif
