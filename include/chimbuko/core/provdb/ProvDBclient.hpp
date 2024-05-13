#pragma once
#include<chimbuko_config.h>

#ifdef ENABLE_PROVDB

#include <sonata/Client.hpp>
#include <nlohmann/json.hpp>
#include <list>
#include <chimbuko/core/ad/ADProvenanceDBengine.hpp>
#include <chimbuko/core/util/PerfStats.hpp>

namespace chimbuko{

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
    std::list<sonata::AsyncRequest> outstanding;

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

    /**
     * @brief Get the number of incomplete (outstanding) requests
     *
     * Non-const because it must purge completed requests from the queue prior to counting
     */
    size_t getNoutstanding();

    ~AnomalousSendManager();
  };


  /**
   * @Client for interacting with provevance database
   */
  class ProvDBclient{
  private:
    sonata::Client m_client; /**< Sonata client */
    sonata::Database m_database; /**< Sonata database */
    std::vector<std::string> m_collection_names; /**< Array of collection names*/
    std::unordered_map<std::string,sonata::Collection> m_collections; /**< Sonata collections mapped by collection name */
    std::string m_instance_descr; /**< The client type*/

    bool m_is_connected; /**< True if connection has been established to the provider */
    
    mutable AnomalousSendManager anom_send_man; /**< Manager for outstanding anomalous requests */

    int m_rank; /**< MPI rank of current process */
    std::string m_server_addr; /**< Address of the server */
    thallium::endpoint m_server; /**< Endpoint for provDB comms*/
    thallium::remote_procedure *m_stop_server; /** RPC to shutdown server*/
#ifdef ENABLE_MARGO_STATE_DUMP
    thallium::remote_procedure *m_margo_dump; /** RPC to request the server dump its state*/
#endif    
    bool m_perform_handshake; /**< Optionally disable the client->server registration */

    PerfStats *m_stats; /**< Performance data gathering*/

    /**
     * @brief Send std::vector of JSON strings asynchronously to the database (non-blocking). This is intended for sending many independent data entries at once.
     * @param entries std::vector of data
     * @param collection The collection to send to
     * @param req Allow querying of the outstanding request and retrieval of ids (once complete). If nullptr the request will be anonymous (fire-and-forget)
     */
    void sendMultipleDataAsync(const std::vector<std::string> &entries, const std::string &collection, OutstandingRequest *req = nullptr) const;
    
  public:
    /**
     * @brief Constructor
     * @param collection_names An array of collection names
     * @param instance_descr A description of this instance, eg "rank 0", used for logging     
     */
    ProvDBclient(const std::vector<std::string> &collection_names, const std::string &instance_descr= "");

    virtual ~ProvDBclient();

    /**
     * @brief Enable or disable the client<->server handshake (call before connecting)
     */
    void setEnableHandshake(const bool to){ m_perform_handshake = to; }

    /**
     * @brief Perform an appropriate hello handshake with the server. This function is called automatically by connect
     */
    virtual void handshakeHello(thallium::engine &eng, thallium::endpoint &server) = 0;

    /**
     * @brief Perform an appropriate goodbye handshake with the server. This function is called automatically by disconnect
     */
    virtual void handshakeGoodbye(thallium::engine &eng, thallium::endpoint &server) = 0;
    
    /**
     * @brief Connect the client to the provenance database server
     * @param addr The server address
     * @param db_name The name of the database
     * @param provider_idx The Sonata provider index     
     */
    void connect(const std::string &addr, const std::string &db_name, const int provider_idx);

    /**
     * @brief Check if connnection has been established to provider
     */
    bool isConnected() const{ return m_is_connected; }
    
    /**
     * @brief Disconnect if presently connected
     */
    void disconnect(); 

    /**
     * @brief Get the Sonata collection associated with the name allowing access to more sophisticated functionality
     */
    sonata::Collection & getCollection(const std::string &name);
    const sonata::Collection & getCollection(const std::string &name) const;
		
    /**
     * @brief Send data JSON objects synchronously to the database (blocking)
     * @param entry JSON data
     * @param collection The collection
     * @return Entry index
     */
    uint64_t sendData(const nlohmann::json &entry, const std::string &collection) const;

    /**
     * @brief Send *JSON array* of JSON objects synchronously to the database (blocking). This is intended for sending many independent data entries at once.
     * @param entries JSON array of data
     * @param collection The collection
     * @return Vector of entry indices. Array will have zero size if database not connected.
     */
    std::vector<uint64_t> sendMultipleData(const nlohmann::json &entries, const std::string &collection) const;


    /**
     * @brief Send std::vector of JSON objects synchronously to the database (blocking). This is intended for sending many independent data entries at once.
     * @param entries std::vector of data
     * @param collection The collection
     * @return Vector of entry indices. Array will have zero size if database not connected.
     */
    std::vector<uint64_t> sendMultipleData(const std::vector<nlohmann::json> &entries, const std::string &collection) const;


    /**
     * @brief Send data JSON objects asynchronously to the database (non-blocking)
     * @param entry JSON data
     * @param collection The collection
     * @param req Allow querying of the outstanding request and retrieval of ids (once complete). If nullptr the request will be anonymous (fire-and-forget)
     */
    void sendDataAsync(const nlohmann::json &entry, const std::string &collection, OutstandingRequest *req = nullptr) const;


    /**
     * @brief Send std::vector of JSON objects asynchronously to the database (non-blocking). This is intended for sending many independent data entries at once.
     * @param entries std::vector of data
     * @param collection The collection
     * @param req Allow querying of the outstanding request and retrieval of ids (once complete). If nullptr the request will be anonymous (fire-and-forget)
     */
    void sendMultipleDataAsync(const std::vector<nlohmann::json> &entries, const std::string &collection, OutstandingRequest *req = nullptr) const;


    /**
     * @brief Send *JSON array* of JSON objects asynchronously to the database (non-blocking). This is intended for sending many independent data entries at once.
     * @param entries JSON array of data
     * @param collection The collection
     * @param req Allow querying of the outstanding request and retrieval of ids (once complete). If nullptr the request will be anonymous (fire-and-forget)
     */
    void sendMultipleDataAsync(const nlohmann::json &entries, const std::string &collection, OutstandingRequest *req = nullptr) const;


    /**
     * @brief Retrieve an inserted JSON object synchronously from the database by index (blocking) (primarily for testing)
     * @param[out] entry The JSON object (if valid)
     * @param[in] index The entry index
     * @param[in] collection The collection
     * @return True if the client was able to retrieve the object
     */
    bool retrieveData(nlohmann::json &entry, uint64_t index, const std::string &collection) const;

    /**
     * @brief Block (wait) until all outstanding anomalous sends have completed
     */
    void waitForSendComplete();

    /**
     * @brief Retrieve all records from the database
     */
    std::vector<std::string> retrieveAllData(const std::string &collection) const;

    /**
     * @brief Apply a jx9 filter to the database and retrieve all records that match
     */
    std::vector<std::string> filterData(const std::string &collection, const std::string &query) const;

    /**
     * @brief Execute arbitrary jx9 code on the database
     * 
     * Note that this acts on the *database* and not the individual collections. The code should access the appropriate collection ("anomalies" or "metadata")
     * @param code Jx9 code to execute
     * @param vars A set of variables that are assigned by the code
     * @return A map between the variables and their values
     */
    std::unordered_map<std::string,std::string> execute(const std::string &code, const std::unordered_set<std::string>& vars) const;

    /**
     * @brief Link a PerfStats instance to monitor performance
     */
    void linkPerf(PerfStats *stats){ m_stats = stats; }

    /**
     * @brief Get the number of incomplete (outstanding) asynchronous stores
     */
    size_t getNoutstandingAsyncReqs(){ return anom_send_man.getNoutstanding(); }


    /**
     * @brief Issue a stop request to the server
     */
    void stopServer() const;

#ifdef ENABLE_MARGO_STATE_DUMP
    /**
     * @brief Issue an RPC request telling the server to dump its state
     */
    void serverDumpState() const;
#endif

  };

};

#endif
