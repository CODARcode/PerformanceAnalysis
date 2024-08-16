#include<chimbuko/modules/performance_analysis/provdb/ProvDBprune.hpp>
#include<chimbuko/modules/performance_analysis/provdb/ProvDBpruneInterface.hpp>
#include<chimbuko/core/util/error.hpp>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

void ProvDBprune::pruneImplementation(ADOutlier &ad, sonata::Database &db){
  //Prune the outliers and update scores / model on remaining
  ProvDBpruneInterface pi(ad, db, ADDataInterface::EventType::Outlier);
  ad.run(pi);

  //Prune normal execs and update scores / model on remaining




  //Reparse outliers collection for statistics


}

void ProvDBprune::finalize(sonata::Database &global_db){
  //Update anomaly information statistics in global database
  
}



