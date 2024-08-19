#pragma once
#include <chimbuko_config.h>

#include <chimbuko/core/util/RunStats.hpp>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <utility>
#include <boost/functional/hash.hpp>

namespace chimbuko {
  namespace modules{
    namespace performance_analysis{

      /**
       * @brief Structure for rebuilding global anomaly statistics for a specific function
       */
      struct ProvDBpruneGlobalStats{
 
	ProvDBpruneGlobalStats();

	/**
	 * @brief Extract metrics information from the provided record
	 */
	void push(const nlohmann::json &record); 

	/**
	 * @brief Generate the anomaly_metrics report for the global database
	 */
	nlohmann::json summarize() const;

      private:
	std::unordered_map<std::pair<int,int>, int, boost::hash<std::pair<int,int> > > rank_anom_counts; //[rank,step]->count
	int first_io_step;
	int last_io_step;
	unsigned long min_timestamp;
	unsigned long max_timestamp;
	RunStats score;
	RunStats severity;
      };
    }
  }
}
