#pragma once
#include<chimbuko_config.h>

#ifdef ENABLE_PROVDB

#include <thallium.hpp>
#include <yokan/cxx/client.hpp>
#include <yokan/cxx/database.hpp>
#include <yokan/cxx/collection.hpp>
#include <nlohmann/json.hpp>
#include <list>
#include <chimbuko/util/PerfStats.hpp>

namespace chimbuko{

  /**
   * @brief A container for an outstanding request and the ids of the DB entries
   */
  struct OutstandingRequest{
    mutable thallium::eventual<void> req; /**< The request instance */
    std::vector<yk_id_t> ids; /**< The ids of the inserted data, *populated only once request is complete* */
    
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
    std::list<thallium::eventual<void> > outstanding;

    /**
     * @brief Remove completed requests if size > MAX_OUTSTANDING
     */
    void purge();
  public:
    /**
     * @brief Get a new AsyncRequest instance reference pointing to an object in the FIFO
     */
    thallium::eventual<void> & getNewRequest();

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

    /**
     * @brief Get list of outstanding requests. For testing only
     */
    std::list<thallium::eventual<void> > & getOutstandingEvents(){ return outstanding; }
  };

  /**
   * @brief Common base class for provenance database clients, providing basic functionality
   */
  class provDBclient{
  protected:
    std::string m_stats_prefix; /**< Prefix text in perf output description*/
    PerfStats *m_stats; /**< Performance data gathering*/
    mutable AnomalousSendManager anom_send_man; /**< Manager for outstanding anomalous requests */

    yokan::Client m_client; /**< Yokan client */
    yokan::Database m_database; /**< Yokan database */

    std::string m_server_addr; /**< Address of the server */
    thallium::endpoint m_server; /**< Endpoint for provDB comms*/
    bool m_is_connected; /**< True if connection has been established to the provider */

    thallium::engine* m_eng; /**< The thallium engine*/

    /**
     * @brief Open a collection on a database
     */
    static yokan::Collection* openCollection(yokan::Database &db, const std::string &coll);

    /**
     * @brief Connect the client to the provenance database server
     * @param addr The server address
     * @param db_name The name of the database
     * @param provider_idx The Sonata provider index     
     */
    void connect(const std::string &addr, const std::string &db_name, const int provider_idx);

    /**
     * @brief Disconnect if presently connected
     */
    void disconnect(); 

    /**
     * @brief Link a PerfStats instance to monitor performance
     * @param stats Pointer to the PerfStats instance
     * @param stats_prefix Prefix text in perf output description
     */
    void linkPerf(PerfStats *stats, const std::string &stats_prefix){ m_stats = stats;  m_stats_prefix = stats_prefix; }
    
    /**
     * @brief Send data JSON objects synchronously to the database (blocking)
     * @param entry JSON data
     * @param coll The collection
     * @return Entry index
     */
    yk_id_t sendData(const nlohmann::json &entry, const yokan::Collection &coll) const;

    /**
     * @brief Send data JSON objects asynchronously to the database (non-blocking)
     * @param entry JSON data
     * @param coll The collection
     * @param req Allow querying of the outstanding request and retrieval of ids (once complete). If nullptr the request will be anonymous (fire-and-forget)
     */
    void sendDataAsync(const nlohmann::json &entry, const yokan::Collection &coll, OutstandingRequest *req = nullptr) const;

    /**
     * @brief Send std::vector of JSON strings synchronously to the database (blocking). This is intended for sending many independent data entries at once.
     * @param entries std::vector of data
     * @param coll The database collection
     * @return Vector of entry indices. Array will have zero size if database not connected.
     */
    std::vector<yk_id_t> sendMultipleData(const std::vector<std::string> &entries, const yokan::Collection &coll) const;

    /**
     * @brief Send *JSON array* of JSON objects synchronously to the database (blocking). This is intended for sending many independent data entries at once.
     * @param entries JSON array of data
     * @param coll The database collection
     * @return Vector of entry indices. Array will have zero size if database not connected.
     */
    std::vector<yk_id_t> sendMultipleData(const nlohmann::json &entries, const yokan::Collection &coll) const;


    /**
     * @brief Send std::vector of JSON objects synchronously to the database (blocking). This is intended for sending many independent data entries at once.
     * @param entries std::vector of data
     * @param coll The database collection
     * @return Vector of entry indices. Array will have zero size if database not connected.
     */
    std::vector<yk_id_t> sendMultipleData(const std::vector<nlohmann::json> &entries, const yokan::Collection &coll) const;



    /**
     * @brief Send std::vector of JSON strings asynchronously to the database (non-blocking). This is intended for sending many independent data entries at once.
     * @param entries std::vector of data
     * @param coll The database collection
     * @param req Allow querying of the outstanding request and retrieval of ids (once complete). If nullptr the request will be anonymous (fire-and-forget)
     */
    void sendMultipleDataAsync(const std::vector<std::string> &entries, const yokan::Collection &coll, OutstandingRequest *req = nullptr) const;

    /**
     * @brief Send std::vector of JSON objects asynchronously to the database (non-blocking). This is intended for sending many independent data entries at once.
     * @param entries std::vector of data
     * @param coll The database collection
     * @param req Allow querying of the outstanding request and retrieval of ids (once complete). If nullptr the request will be anonymous (fire-and-forget)
     */
    void sendMultipleDataAsync(const std::vector<nlohmann::json> &entries, const yokan::Collection &coll, OutstandingRequest *req = nullptr) const;


    /**
     * @brief Send *JSON array* of JSON objects asynchronously to the database (non-blocking). This is intended for sending many independent data entries at once.
     * @param entries JSON array of data
     * @param coll The database collection
     * @param req Allow querying of the outstanding request and retrieval of ids (once complete). If nullptr the request will be anonymous (fire-and-forget)
     */
    void sendMultipleDataAsync(const nlohmann::json &entries, const yokan::Collection &coll, OutstandingRequest *req = nullptr) const;

    /**
     * @brief Retrieve an inserted JSON object synchronously from the database by index (blocking) (primarily for testing)
     * @param[out] entry The JSON object (if valid)
     * @param[in] index The entry index
     * @param[in] coll The database collection
     * @return True if the client was able to retrieve the object
     */
    bool retrieveData(nlohmann::json &entry, yk_id_t index, const yokan::Collection &coll) const;


    /**
     * @brief Retrieve all records from the database
     */
    std::vector<std::string> retrieveAllData(const yokan::Collection &coll) const;

    /**
     * @brief Remove all records from the database
     */
    void clearAllData(const yokan::Collection &coll) const;
    
    /**
     * @brief Apply a lua filter to the database and retrieve all records that match
     */
    std::vector<std::string> filterData(const yokan::Collection &coll, const std::string &query) const;


  public:
    /**
     * @brief Constructor
     */
    provDBclient();

    ~provDBclient();

    /**
     * @brief Check if connnection has been established to provider
     */
    bool isConnected() const{ return m_is_connected; }

    /**
     * @brief Block (wait) until all outstanding anomalous sends have completed
     */
    void waitForSendComplete();

    /**
     * @brief Get the number of incomplete (outstanding) asynchronous stores
     */
    size_t getNoutstandingAsyncReqs(){ return anom_send_man.getNoutstanding(); }
    
    /**
     * @brief Get the anomalous send manager. Should be used for testing only
     */
    AnomalousSendManager & getAnomSendManager(){ return anom_send_man; }
  };




}

#endif
