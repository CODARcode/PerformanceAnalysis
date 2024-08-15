#include<chimbuko/core/provdb/ProvDBpruneCore.hpp>
#include<chimbuko/core/verbose.hpp>
#include<chimbuko/core/util/error.hpp>

using namespace chimbuko;

void ProvDBpruneCore::prune(const std::string &algorithm, const ADOutlier::AlgoParams &algo_params, const std::string &params_ser, sonata::Database &db){
  std::unique_ptr<ADOutlier> ad(ADOutlier::set_algorithm(0,algorithm,algo_params));
  ad->setGlobalParameters(params_ser); //input model
  ad->setGlobalModelSyncFrequency(0); //fix model

  this->pruneImplementation(*ad, db);
}
