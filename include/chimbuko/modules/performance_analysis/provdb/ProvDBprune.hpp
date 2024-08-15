#pragma once

#include <chimbuko_config.h>
#include <chimbuko/core/provdb/ProvDBpruneCore.hpp>

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

      };  

    }
  }
}
