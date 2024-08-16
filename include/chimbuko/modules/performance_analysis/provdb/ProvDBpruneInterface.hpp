#pragma once
#include <chimbuko_config.h>
#include <chimbuko/core/ad/ADDataInterface.hpp>
#include <chimbuko/core/ad/ADOutlier.hpp>

#include<string>
#include <sonata/Database.hpp>

namespace chimbuko {
  namespace modules{
    namespace performance_analysis{

      /**
       * @brief The interface class between the provDB data and the AD algorithm
       */
      class ProvDBpruneInterface: public ADDataInterface{
      public:
	ProvDBpruneInterface(const ADOutlier &ad, sonata::Database &db);

	/**
	 * @brief Get the values associated with each recorded anomaly
	 */
	std::vector<Elem> getDataSet(size_t dset_index) const override;
	
	/**
	 * @brief Check the newly assigned label is still anomaly, otherwise erase the element from the database
	 */
	void recordDataSetLabelsInternal(const std::vector<Elem> &data, size_t dset_index) override;

	
	/**
	 * @brief Return the function index (a unique index associated with a program index/function name combination) associated with a given dataset index
	 */
	size_t getDataSetModelIndex(size_t dset_index) const;


      private:
	sonata::Database &m_database;
	std::unique_ptr<sonata::Collection> m_collection;
	std::unordered_map<unsigned long, std::vector<std::pair<uint64_t, double> > > m_data; //[fid] -> [  (record_id, value), ... ]
	const ADOutlier &m_ad; //the outlier algorithm to allow access to the model data when updating records
      };    

    }
  }
}
