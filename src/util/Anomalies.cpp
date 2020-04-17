#include "chimbuko/util/Anomalies.hpp"

using namespace chimbuko;

void Anomalies::insert(CallListIterator_t outlier){
  m_all_outliers.push_back(outlier);
  m_func_outliers[outlier->get_fid()].push_back(outlier);
}
    
const std::vector<CallListIterator_t> & Anomalies::funcOutliers(const unsigned long func_id) const{
  static std::vector<CallListIterator_t> empty;
  auto it = m_func_outliers.find(func_id);
  if(it != m_func_outliers.end()) return it->second;
  else return empty;
}
