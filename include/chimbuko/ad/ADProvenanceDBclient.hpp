#pragma once
#include<config.h>

#ifdef ENABLE_PROVDB

#include <sonata/Client.hpp>
#include <nlohmann/json.hpp>
#include <queue>
#include <chimbuko/ad/ADProvenanceDBengine.hpp>

namespace chimbuko{

  enum class ProvenanceDataType { AnomalyData, Metadata };

  /**
   * @brief A container for an outstanding request and the ids of the DB entries
   */
  struct OutstandingRequest{
    sonata::AsyncRequest req; /**< The request instance */
    std::vector<uint64_t> ids; /**< The ids of the inserted data, *populated only once request is complete* */
    
    /**
     * @brief Default constructor. For async sends it is safe to use this as the ids array is resized by the send functions
     */
    OutstandingRequest(){}

    /**
     * @brief Block until the request is complete
     */
    inline void wait() const{
      req.wait();
    }
  };


  /**
   * @brief Manager class for containing outstanding anomalous requests
   *
   * Sonata requires an instance of AsyncRequest live for long enough that the request completes
   * If we are not interested in retrieving the data again we can just store them somewhere and periodically purge them
   */
  class AnomalousSendManager{
    static const int MAX_OUTSTANDING = 100;
    std::queue<sonata::AsyncRequest> outstanding;

    /**
     * @brief Remove completed requests if size > MAX_OUTSTANDING
     */
    void purge();
  public:
    /**
     * @brief Get a new AsyncRequest instance reference pointing to an object in the FIFO
     */
    sonata::AsyncRequest & getNewRequest();

    /**
     * @brief Block (wait) until all outstanding requests have completed
     */
    void waitAll();

    ~AnomalousSendManager();
  };


  /**
   * @Client for interacting with provevance database
   */
  class ADProvenanceDBclient{
  private:
    sonata::Client m_client; /**< Sonata client */
    sonata::Database m_database; /**< Sonata database */
    sonata::Collection m_coll_anomalies; /**< The anomalies collection */
    sonata::Collection m_coll_metadata; /**< The metadata collection */
    bool m_is_connected; /**< True if connection has been established to the provider */
    
    static AnomalousSendManager anom_send_man; /**< Manager for outstanding anomalous requests */
  public:
    ADProvenanceDBclient(): m_is_connected(false){}

    ~ADProvenanceDBclient();
    
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
     * @brief Get the Sonata collection associated with the data type allowing access to more sophisticated functionality
     */
    inline sonata::Collection & getCollection(const ProvenanceDataType type){ return type == ProvenanceDataType::AnomalyData ? m_coll_anomalies : m_coll_metadata; }
    inline const sonata::Collection & getCollection(const ProvenanceDataType type) const{ return type == ProvenanceDataType::AnomalyData ? m_coll_anomalies : m_coll_metadata; }
		
    /**
     * @brief Send data JSON objects synchronously to the database (blocking)
     * @param entry JSON data
     * @param type The data type
     * @return Entry index
     */
    uint64_t sendData(const nlohmann::json &entry, const ProvenanceDataType type) const;

    /**
     * @brief Send *JSON array* of JSON objects synchronously to the database (blocking). This is intended for sending many independent data entries at once.
     * @param entries JSON array of data
     * @param type The data type
     * @return Vector of entry indices. Array will have zero size if database not connected.
     */
    std::vector<uint64_t> sendMultipleData(const nlohmann::json &entries, const ProvenanceDataType type) const;


    /**
     * @brief Send std::vector of JSON objects synchronously to the database (blocking). This is intended for sending many independent data entries at once.
     * @param entries std::vector of data
     * @param type The data type
     * @return Vector of entry indices. Array will have zero size if database not connected.
     */
    std::vector<uint64_t> sendMultipleData(const std::vector<nlohmann::json> &entries, const ProvenanceDataType type) const;


    /**
     * @brief Send data JSON objects asynchronously to the database (non-blocking)
     * @param entry JSON data
     * @param type The data type
     * @param req Allow querying of the outstanding request and retrieval of ids (once complete). If nullptr the request will be anonymous (fire-and-forget)
     */
    void sendDataAsync(const nlohmann::json &entry, const ProvenanceDataType type, OutstandingRequest *req = nullptr) const;


    /**
     * @brief Send std::vector of JSON objects asynchronously to the database (non-blocking). This is intended for sending many independent data entries at once.
     * @param entries std::vector of data
     * @param type The data type
     * @param req Allow querying of the outstanding request and retrieval of ids (once complete). If nullptr the request will be anonymous (fire-and-forget)
     */
    void sendMultipleDataAsync(const std::vector<nlohmann::json> &entries, const ProvenanceDataType type, OutstandingRequest *req = nullptr) const;


    /**
     * @brief Send *JSON array* of JSON objects asynchronously to the database (non-blocking). This is intended for sending many independent data entries at once.
     * @param entries JSON array of data
     * @param type The data type
     * @param req Allow querying of the outstanding request and retrieval of ids (once complete). If nullptr the request will be anonymous (fire-and-forget)
     */
    void sendMultipleDataAsync(const nlohmann::json &entries, const ProvenanceDataType type, OutstandingRequest *req = nullptr) const;


    /**
     * @brief Retrieve an inserted JSON object synchronously from the database by index (blocking) (primarily for testing)
     * @param[out] entry The JSON object (if valid)
     * @param[in] index The entry index
     * @param[in] type The data type
     * @return True if the client was able to retrieve the object
     */
    bool retrieveData(nlohmann::json &entry, uint64_t index, const ProvenanceDataType type) const;

    /**
     * @brief Block (wait) until all outstanding anomalous sends have completed
     */
    void waitForSendComplete();

    /**
     * @brief Retrieve all records from the database
     */
    std::vector<std::string> retrieveAllData(const ProvenanceDataType type) const;

    /**
     * @brief Apply a jx9 filter to the database and retrieve all records that match
     */
    std::vector<std::string> filterData(const ProvenanceDataType type, const std::string &query) const;

    /**
     * @brief Execute arbitrary jx9 code on the database
     * 
     * Note that this acts on the *database* and not the individual collections. The code should access the appropriate collection ("anomalies" or "metadata")
     * @param code Jx9 code to execute
     * @param vars A set of variables that are assigned by the code
     * @return A map between the variables and their values
     */
    std::unordered_map<std::string,std::string> execute(const std::string &code, const std::unordered_set<std::string>& vars) const;

  };

};

#endif
