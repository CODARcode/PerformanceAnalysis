#pragma once

#include <chimbuko_config.h>
#include <chimbuko/util/error.hpp>
#include <fstream>

namespace chimbuko{

  /**
   * @brief A class that computes the assignment of shards to server instances and provider indices
   */
  class ProvDBsetup{
    int m_nshards; /**< Number of shards */
    int m_ninstances; /**< Number of database instances */

    int m_nshard_instance; /** Number of shards per database instance (base, may be less for final shard if #shards not divisible by #instances)*/

  public:
    /**
     * @brief Instantiate the object for a chosen number of shards and instances
     *
     * @param nshards The total number of shards (over all instances)
     * @param ninstances The number of server instances
     */
    ProvDBsetup(const int nshards, const int ninstances): m_nshards(nshards), m_ninstances(ninstances){
      if(ninstances <1) fatal_error("Number of instances must be >=1");
      if(nshards<0)  fatal_error("Number of shards must be >=1");
      m_nshard_instance = ( nshards + ninstances - 1 ) /  ninstances;
    }

    /**
     * @brief Check instance index in range
     */
    inline void checkInstance(const int instance) const{
      if(instance<0 || instance >= m_ninstances) fatal_error("Invalid instance index");
    }

    /**
     * @brief Check shard index in range
     */
    inline void checkShard(const int shard) const{
      if(shard < 0 || shard >= m_nshards) fatal_error("Invalid shard index");
    }

    /**
     * @brief Get the filename containing the ip address of the instance
     */
    static inline std::string getInstanceAddressFilename(const int instance, const std::string &dir = "."){
      return dir + "/provider.address." + std::to_string(instance);
    }

    /**
     * @brief Get the ip address for an instance given the directory in which the address files are written
     */
    static inline std::string getInstanceAddress(const int instance, const std::string &addr_file_dir = "."){
      std::string fn = getInstanceAddressFilename(instance, addr_file_dir);
      std::ifstream in(fn);
      if(in.bad()){ fatal_error("Could not open address file " + fn); }
      std::string addr; in >> addr;
      if(in.bad()){ fatal_error("Could not read address file " + fn); }
      return addr;
    }

    /**
     * @brief Get the integer offset of the first shard on a given instance
     */
    inline int getShardOffsetInstance(const int instance) const{
      checkInstance(instance);
      return instance * m_nshard_instance;
    }

    /**
     * @brief Get the number of shards serviced by a given instance
     */
    inline int getNshardsInstance(const int instance) const{
      checkInstance(instance);
      return instance == (m_ninstances - 1) ? 
	(m_nshards - m_nshard_instance * instance) 
	: 
	m_nshard_instance;
    }
    
    /**
     * @brief Get the instance responsible for the global database
     */
    static inline int getGlobalDBinstance(){
      return 0;
    }
    
    /** 
     * @brief Get the instance responsible for a given shard
     */
    inline int getShardInstance(const int shard) const{
      if(shard < 0 || shard >= m_nshards) fatal_error("Invalid shard index");
      return shard / m_nshard_instance;
    }

    /**
     * @brief Get the provider index associated with the global database (on the instance responsible for it)
     */
    static inline int getGlobalDBproviderIndex(){
      return 0;
    }

    /**
     * @brief Get the provider index associated with a given shard
     */
    inline int getShardProviderIndex(const int shard) const{
      int instance = getShardInstance(shard);
      int off = shard % m_nshard_instance;
      return instance == getGlobalDBinstance() ? off + 1 : off; //only one instance hosts the global database
    }

    /**
     * @brief Get the name of the Sonata database corresponding to a particular shard
     */
    static inline std::string getShardDBname(const int shard){
      return "provdb." + std::to_string(shard);
    }

    /**
     * @brief Get the name of the Sonata database corresponding to the global database
     */   
    static inline std::string getGlobalDBname(){
      return "provdb.global";
    }
	

  };

};

