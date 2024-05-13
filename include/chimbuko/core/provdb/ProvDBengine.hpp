#pragma once
#include<chimbuko_config.h>

#ifdef ENABLE_PROVDB

#include <sonata/Client.hpp>

namespace chimbuko{


  /**
   * @Class for managing singleton instance of thallium engine
   */
  class ProvDBengine{
  private:
    /**
     *@brief Internal structure containing engine and parameters
     */
    struct data_v{
      thallium::engine* m_eng;
      std::pair<std::string, int> m_protocol; /**< The protocol and mode (client/server)*/
      std::string m_mercury_auth_key; /**< Mercury authorization key (optional)*/
      
      bool m_is_initialized;
      inline data_v(): m_eng(nullptr), m_protocol({"ofi+tcp;ofi_rxm", THALLIUM_CLIENT_MODE}), m_mercury_auth_key(""), m_is_initialized(false){}

      /**
       * @brief Initialize the engine (if not already initialized)
       */
      void initialize();
      
      /**
       * @brief Finalize the engine (automatically called by destructor)
       */
      void finalize();

      inline ~data_v(){
	finalize();
       }
    };

    /**
     * @brief Access the data structure
     */
    static inline data_v & data(){ 
      static data_v m_data;
      return m_data;
    }
  public:
    /**
     * @brief Set the protocol used to create the engine. This must be used *before* the engine is created to have any effect
     */
    static inline void setProtocol(const std::string &transport, const int mode){
      data().m_protocol = {transport, mode};
    }

    /**
     * @brief Set the Mercury authorization key. Must be done before the engine is created
     */
    static inline void setMercuryAuthorizationKey(const std::string &to){
      data().m_mercury_auth_key = to;
    }
    
    /**
     * @brief Get the protocol used to create the engine.
     */
    static inline std::pair<std::string, int> getProtocol(){
      return data().m_protocol;
    }

    /**
     * @bried Get the engine. Initializes if not already done so
     */
    static inline thallium::engine & getEngine(){
      data().initialize(); //only if !initialized
      return *data().m_eng;
    }
    /**
     * @brief Finalize the engine. This happens automatically at program exit if not previously called
     */
    static inline void finalize(){
      data().finalize();
    }

    /**
     * @brief Get the protocol from the address string
     */
    static std::string getProtocolFromAddress(const std::string &addr);
    

  };




}

#endif
