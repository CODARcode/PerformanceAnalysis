#include<chimbuko/modules/performance_analysis/provdb/ProvDBpruneOutlierInterface.hpp>
#include<chimbuko/core/util/error.hpp>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

ProvDBpruneOutlierInterface::ProvDBpruneOutlierInterface(sonata::Database &db): m_database(db), ADDataInterface(){
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

    //m_data[ strToAny<unsigned long>(e[0]) ].push_back(std::pair<size_t,double>(strToAny<size_t>(e[1]), strToAny<double>(e[2])));
  
  for(auto const &d : m_data){
    std::cout << d.first << ":";
    for(auto const &e : d.second) std::cout << "(" << e.first << "," << e.second << ")" << " ";
    std::cout << std::endl;
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

  for(auto const &e: data){ //we used the record id as the index, which is unique
    if(e.label == ADDataInterface::EventType::Normal){
      std::cout << "Pruning record " << e.index << " with value " << e.value << " and score " << e.score << std::endl;
      to_prune.push_back(e.index);
    }
  }
  m_collection->erase_multi(to_prune.data(), to_prune.size(), true); //last entry tells database to commit change
}

size_t ProvDBpruneOutlierInterface::getDataSetModelIndex(size_t dset_index) const{
  return std::next(m_data.begin(), dset_index)->first;
}


void chimbuko::modules::performance_analysis::ProvDBpruneOutliers(const std::string &algorithm, const ADOutlier::AlgoParams &algo_params, 
								  const std::string &params_ser, sonata::Database &db){
  std::unique_ptr<ADOutlier> ad(ADOutlier::set_algorithm(0,algorithm,algo_params));
  ad->setGlobalParameters(params_ser); //input model
  ad->setGlobalModelSyncFrequency(0); //fix model
  ProvDBpruneOutlierInterface pi(db);
  ad->run(pi);
}
  
