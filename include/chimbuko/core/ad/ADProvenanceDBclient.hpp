#pragma once
#include<chimbuko_config.h>

#ifdef ENABLE_PROVDB

#include <chimbuko/core/provdb/ProvDBclient.hpp>

namespace chimbuko{
  /**
   * @Client for interacting with provevance database
   */
  class ADProvenanceDBclient: public ProvDBclient{
  private:
    int m_rank; /**< MPI rank of current process */
    thallium::remote_procedure *m_client_hello; /**< RPC to register client with provDB */
    thallium::remote_procedure *m_client_goodbye; /**< RPC to deregister client with provDB */
   
  public:
    /**
     * @brief Constructor
     * @param collection_names An array of collection names
     * @param rank The rank of the current process
     */
    ADProvenanceDBclient(const std::vector<std::string> &collection_names, int rank);

    ~ADProvenanceDBclient();

    /**
     * @brief Connect the client to the provenance database server
     * @param addr The server address
     * @param nshards the number of database shards. Connection to shard will be round-robin by rank
     *
     * This version assumes a single database server instance on the provided address     
     */
    void connectSingleServer(const std::string &addr, const int nshards);

    /**
     * @brief Connect the client to the provenance database server instance in a multi-instance setup
     * @param addr_file_dir The directory in which the server addresses are output by the provDB
     * @param nshards The number of database shards. Connection to shard will be round-robin by rank
     * @param ninstance The number of server instances
     */
    void connectMultiServer(const std::string &addr_file_dir, const int nshards, const int ninstances);

    /**
     * @brief Connect the client to the provenance database server instance in a multi-instance setup using a specific assignment of rank to shard given in an input file
     * @param addr_file_dir The directory in which the server addresses are output by the provDB
     * @param nshards The number of database shards. Connection to shard will be round-robin by rank
     * @param ninstance The number of server instances
     * @param shard_assign_file A file containing the shard used by each rank. The file format should have 1 line per rank, in ascending order, containing just the shard index
     */
    void connectMultiServerShardAssign(const std::string &addr_file_dir, const int nshards, const int ninstances, const std::string &shard_assign_file);

    /**
     * @brief Perform an appropriate hello handshake with the server. This function is called automatically by connect
     */
    void handshakeHello(thallium::engine &eng, thallium::endpoint &server) override;

    /**
     * @brief Perform an appropriate goodbye handshake with the server. This function is called automatically by disconnect
     */
    void handshakeGoodbye(thallium::engine &eng, thallium::endpoint &server) override;
  };

};

#endif
