#include<chimbuko/modules/performance_analysis/provdb/ProvDBpruneOutlierInterface.hpp>
#include<chimbuko/core/provdb/ProvDButils.hpp>
#include<chimbuko/core/util/error.hpp>
#include<chimbuko/core/verbose.hpp>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

ProvDBpruneOutlierInterface::ProvDBpruneOutlierInterface(const ADOutlier &ad, sonata::Database &db): m_database(db), m_ad(ad), ADDataInterface(){
  m_collection.reset(new sonata::Collection(db.open("anomalies")));
  //To avoid loading all items into memory we must loop over the database using its jx9 interface
  std::string script = R"(
$ret = [];
while( ($rec = db_fetch('anomalies')) != NULL ){
  $r = [ $rec.__id, $rec.fid, $rec.runtime_exclusive ];
  array_push($ret, $r);
}
)";
  std::unordered_map<std::string,std::string> result;
  db.execute(script, {"ret"}, &result);

  auto r = result.find("ret");
  if(r == result.end()) fatal_error("JX9 script failed to execute correctly");
  
  nlohmann::json rj = nlohmann::json::parse(r->second);
  if(!rj.is_array()) fatal_error("Expected an array type");
  
  for(int i=0;i<rj.size();i++)
    std::cout << i << " " << rj[i][0] << " " << rj[i][1] << " " << rj[i][2] << std::endl;
  
  for(auto const &e: rj){
    uint64_t id = e[0].template get<uint64_t>();
    unsigned long fid = e[1].template get<unsigned long>();
    double val = e[2].template get<double>();
    m_data[fid].push_back(std::pair<size_t,double>(id,val) );
  }
  this->setNdataSets(m_data.size());
}

std::vector<ADDataInterface::Elem> ProvDBpruneOutlierInterface::getDataSet(size_t dset_index) const{
  auto it = std::next(m_data.begin(), dset_index);
  std::vector<ADDataInterface::Elem> out(it->second.size(), ADDataInterface::Elem(0,0));
  for(size_t i=0;i<out.size();i++){
    out[i].index = it->second[i].first;
    out[i].value = it->second[i].second;
  }
  return out;
}

void ProvDBpruneOutlierInterface::recordDataSetLabelsInternal(const std::vector<Elem> &data, size_t dset_index){
  std::vector<uint64_t> to_prune;
  std::vector<uint64_t> to_update;
  std::vector<double> update_scores;

  for(auto const &e: data){ //we used the record id as the index, which is unique
    if(e.label == ADDataInterface::EventType::Normal){
      verboseStream << "Pruning record " << e.index << " with value " << e.value << " and score " << e.score << std::endl;
      to_prune.push_back(e.index);
    }else{
      to_update.push_back(e.index);
      update_scores.push_back(e.score);
    }
  }
  progressStream << "Pruning " << to_prune.size() << " of " << data.size() << " records in shard" << std::endl;

  m_collection->erase_multi(to_prune.data(), to_prune.size(), true); //last entry tells database to commit change

  //Grab records to update in batches
  progressStream << "Updating scores and models for " << to_update.size() << " records" << std::endl;
  std::unordered_map<uint64_t, nlohmann::json> json_param_cache;
  batchAmendRecords(*m_collection, to_update, [&](nlohmann::json &rec, size_t i){  
      //Update score
      rec["outlier_score"] = update_scores[i]; //update the score

      //Update algo_params
      uint64_t fid = rec["fid"].template get<uint64_t>();
      auto fit = json_param_cache.find(fid);
      if(fit == json_param_cache.end())
	fit = json_param_cache.insert({fid, m_ad.get_global_parameters()->get_algorithm_params(fid)}).first;
      
      rec["algo_params"] = fit->second;
    }); 
}

size_t ProvDBpruneOutlierInterface::getDataSetModelIndex(size_t dset_index) const{
  return std::next(m_data.begin(), dset_index)->first;
}
