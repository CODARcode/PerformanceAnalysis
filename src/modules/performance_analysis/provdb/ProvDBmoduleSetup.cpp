#include<chimbuko/modules/performance_analysis/provdb/ProvDBmoduleSetup.hpp>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

std::vector<std::string> ProvDBmoduleSetup::getMainDBcollections() const{ return {"anomalies", "metadata", "normalexecs"}; }

std::vector<std::string> ProvDBmoduleSetup::getGlobalDBcollections() const{ return {"func_stats", "counter_stats", "ad_model"}; }

std::vector< std::vector<std::string> > ProvDBmoduleSetup::getCollectionFilterKeys(const std::string &coll, bool is_global) const{
  if(is_global && coll == "func_stats")
    return std::vector< std::vector<std::string> >({ {"anomaly_metrics","severity","accumulate"}, {"anomaly_metrics","score","accumulate"} });
  return std::vector< std::vector<std::string> >();
}
