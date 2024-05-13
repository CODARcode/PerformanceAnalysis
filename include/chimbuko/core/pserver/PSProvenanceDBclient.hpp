#pragma once
#include<chimbuko_config.h>

#ifdef ENABLE_PROVDB

#include <chimbuko/core/provdb/ProvDBclient.hpp>


namespace chimbuko{

  /**
   * @Client for the pserver interaction with the provevance database
   */
  class PSProvenanceDBclient: public ProvDBclient{
  private:
    thallium::remote_procedure *m_client_hello; /**< RPC to register client with provDB */
    thallium::remote_procedure *m_client_goodbye; /**< RPC to deregister client with provDB */

  public:
    PSProvenanceDBclient(const std::vector<std::string> &collections): m_client_hello(nullptr), m_client_goodbye(nullptr), ProvDBclient(collections,"pserver"){}

    ~PSProvenanceDBclient();

    /**
     * @brief Connect the client to the provenance database server
     * @param addr The server address
     * @param provider_idx The provider index on the server
     */
    void connectServer(const std::string &addr, const int provider_idx = 0);

    /**
     * @brief Connect the client the appropriate provenance database server instance using the default setup
     * @param addr_file_dir The directory containing the address files created by the provDB
     */
    void connectMultiServer(const std::string &addr_file_dir);

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
