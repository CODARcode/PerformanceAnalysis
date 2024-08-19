#pragma once

#include <chimbuko_config.h>
#include <chimbuko/core/ad/ADOutlier.hpp>

#include <string>
#include <sonata/Database.hpp>

namespace chimbuko {
  
  class ProvDBpruneCore{
  public:
    ProvDBpruneCore(const std::string &algorithm, const ADOutlier::AlgoParams &algo_params, const std::string &model_ser);

    void prune(sonata::Database &db);
    
    /**
     * @brief Module implementation of pruning given the database shard. This same instance of ProvDBpruneCore will be called in turn for all shards
     *        hence internally stored information can be aggregated
     */
    virtual void pruneImplementation(ADOutlier &ad, sonata::Database &db) = 0;

    /**
     * @brief Optionally perform a finalization which may include updating the information in the global database
     */
    virtual void finalize(sonata::Database &global_db){}

  public:
    std::unique_ptr<ADOutlier> m_outlier;
  };  

}
