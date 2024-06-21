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
      class ProvDBpruneOutlierInterface: public ADDataInterface{
      public:
	ProvDBpruneOutlierInterface(sonata::Database &db);

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
      };


      /**
       * @brief Instantiate the AD algorithm with the provided parameters and use it to prune entries from the database that are no longer outliers
       */
      void ProvDBpruneOutliers(const std::string &algorithm, const ADOutlier::AlgoParams &algo_params, const std::string &params_ser, sonata::Database &db);
      

    }
  }
}
