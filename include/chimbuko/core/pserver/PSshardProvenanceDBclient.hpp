#pragma once
#include<chimbuko_config.h>

#ifdef ENABLE_PROVDB

#include <chimbuko/core/provdb/ProvDBclient.hpp>
#include <chimbuko/core/provdb/setup.hpp>

namespace chimbuko{

  /**
   * @brief A class for connecting the pserver to the sharded main database. No handshake is used because it is assumed these clients are used prior to the pserver global database client being disconnected
   */
  class PSshardProvenanceDBclient: public ProvDBclient{
  public:
    PSshardProvenanceDBclient(const std::vector<std::string> &collections): ProvDBclient(collections,"pserver_shard"){}

    ~PSshardProvenanceDBclient();

    /**
     * @brief Connect the client the appropriate provenance database server instance / shard using the default setup
     * @param addr_file_dir The directory containing the address files created by the provDB
     */
    void connectShard(const std::string &addr_file_dir, int shard, int nshards, int ninstances);

    /**
     * @brief No handshake is needed
     */
    void handshakeHello(thallium::engine &eng, thallium::endpoint &server) override{};

    /**
     * @brief No handshake is needed.
     */
    void handshakeGoodbye(thallium::engine &eng, thallium::endpoint &server) override{};

  };

}

#endif
    
