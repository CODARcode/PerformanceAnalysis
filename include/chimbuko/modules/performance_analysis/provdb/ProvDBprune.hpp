#pragma once

#include <chimbuko_config.h>
#include <chimbuko/core/provdb/ProvDBpruneCore.hpp>
#include <chimbuko/core/util/RunStats.hpp>
#include <chimbuko/modules/performance_analysis/provdb/ProvDBpruneGlobalStats.hpp>
#include <boost/functional/hash.hpp>
#include <utility>
#include <unordered_map>

namespace chimbuko {
  namespace modules{
    namespace performance_analysis{

      class ProvDBprune: public ProvDBpruneCore{
      public:  
	/**
	 * @brief Prune the database shard. Both the anomalies and normalexecs will be updated
	 */
	void pruneImplementation(ADOutlier &ad, sonata::Database &db) override;

	/**
	 * @brief Update the global database with the new statistics information, overriding the old
	 */
	void finalize(sonata::Database &global_db) override;
      private:
        std::unordered_map<unsigned long, ProvDBpruneGlobalStats> m_anom_metrics; /**< Map of fid -> anomaly metrics*/
      };  

    }
  }
}
