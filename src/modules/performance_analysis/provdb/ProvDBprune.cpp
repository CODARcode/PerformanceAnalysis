#include<chimbuko/modules/performance_analysis/provdb/ProvDBprune.hpp>
#include<chimbuko/modules/performance_analysis/provdb/ProvDBpruneInterface.hpp>
#include<chimbuko/core/util/error.hpp>
#include<chimbuko/core/verbose.hpp>
#include<limits>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;
 
void ProvDBprune::pruneImplementation(ADOutlier &ad, sonata::Database &db){
  //Prune the outliers and update scores / model on remaining. Also gather new anomaly statistics
  {
    ProvDBpruneInterface po(ad, db, ADDataInterface::EventType::Outlier, &m_anom_metrics);
    progressStream << "Pruning normal events from 'anomalies' database with " << po.nDataSets() << " function indices" << std::endl;
    ad.run(po);
  }
  //Prune normal execs and update scores / model on remaining
  {
    ProvDBpruneInterface pn(ad, db, ADDataInterface::EventType::Normal);
    progressStream << "Pruning outlier events from 'normal_execs' database with " << pn.nDataSets() << " function indices" << std::endl;
    ad.run(pn);
  }
}

void ProvDBprune::finalize(sonata::Database &global_db){
  //Update anomaly information statistics in global database
  sonata::Collection coll = global_db.open("func_stats");
  //fetch, modify, update
  std::vector<std::string> rstr;
  coll.all(&rstr);
  std::vector<uint64_t> ids(rstr.size());

  for(size_t i = 0; i < rstr.size(); i++){
    nlohmann::json rec = nlohmann::json::parse(rstr[i]);
    unsigned long fid = rec["fid"].template get<unsigned long>();
    ids[i] = rec["__id"].template get<uint64_t>();

    auto it = m_anom_metrics.find(fid);

    //If there are no anomalies for this function we should replace anomaly_metrics with a blank entry
    ProvDBpruneGlobalStats stats;
    if(it != m_anom_metrics.end()) stats = std::move(it->second);
    rec["anomaly_metrics"] = stats.summarize();
    rstr[i] = rec.dump();
  }
  std::vector<bool> updated;
  coll.update_multi(ids.data(), rstr, &updated, true);
}



