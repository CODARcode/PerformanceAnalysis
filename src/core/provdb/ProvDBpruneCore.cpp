#include<chimbuko/core/provdb/ProvDBpruneCore.hpp>
#include<chimbuko/core/verbose.hpp>
#include<chimbuko/core/util/error.hpp>

using namespace chimbuko;

ProvDBpruneCore::ProvDBpruneCore(const ADOutlier::AlgoParams &algo_params, const std::string &model_ser): m_outlier(ADOutlier::set_algorithm(0,algo_params)){
  m_outlier->setGlobalParameters(model_ser); //input model
  m_outlier->setGlobalModelSyncFrequency(0); //fix model
}
  
void ProvDBpruneCore::prune(sonata::Database &db){
  this->pruneImplementation(*m_outlier, db);
}
