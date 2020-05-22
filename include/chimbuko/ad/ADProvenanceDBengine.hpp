#pragma once
#include<config.h>

#ifdef ENABLE_PROVDB

#include <sonata/Client.hpp>

namespace chimbuko{


  /**
   * @Class for managing singleton instance of thallium engine
   */
  class ADProvenanceDBengine{
  private:
    /**
     *@brief Internal structure containing engine and parameters
     */
    struct data_v{
      thallium::engine* m_eng;
      std::pair<std::string, int> m_protocol;
      bool m_is_initialized;
      inline data_v(): m_eng(nullptr), m_protocol({"ofi+tcp", THALLIUM_CLIENT_MODE}), m_is_initialized(false){}

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

  };




}

#endif
