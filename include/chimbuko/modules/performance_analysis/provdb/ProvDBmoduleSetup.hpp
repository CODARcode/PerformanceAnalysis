#pragma once
#include <chimbuko_config.h>
#include <chimbuko/core/provdb/ProvDBmoduleSetupCore.hpp>

namespace chimbuko {
  namespace modules{
    namespace performance_analysis{


      class ProvDBmoduleSetup: public ProvDBmoduleSetupCore{
      public:
	/**
	 * @brief Return an array of collection names for those associated with provenance data provided by the AD clients
	 */
	std::vector<std::string> getMainDBcollections() const override;


	/**
	 * @brief Return an array of collection names for those associated with provenance data provided by the parameter server (i.e. the "global" database)
	 */
	std::vector<std::string> getGlobalDBcollections() const override;

	/**
	 * @brief Return an array of keys for sorting the results of filtering a particular collection
	 * @param coll The collection
	 * @param is_global Indicates whether the database to which the collection belongs is the global database (true) or the main database (false)
	 * @return An array of keys. To accommodate nested data structures, each key is a vector of strings where each string maps to a sub-array key-index, eg {"data","metrics","value"} ->   j["data"]["metrics"]["value"]
	 */
	std::vector< std::vector<std::string> > getCollectionFilterKeys(const std::string &coll, bool is_global = false) const override;

      };

    };
  }
}
