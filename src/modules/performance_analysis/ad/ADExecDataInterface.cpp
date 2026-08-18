#include <chimbuko/modules/performance_analysis/ad/ADExecDataInterface.hpp>
#include <chimbuko/core/verbose.hpp>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

ADExecDataInterface::ADExecDataInterface(ExecDataMap_t const* execDataMap, const std::pair<OutlierStatistic,unsigned long> &stat): m_execDataMap(execDataMap), m_statistic(stat), ADDataInterface(execDataMap->size()), m_dset_fid_map(execDataMap->size()), m_ignore_first_func_call(false){
  //Build a map between a data set index and the function indices
  size_t dset_idx = 0;
  for(auto it = execDataMap->begin(); it != execDataMap->end(); ++it)
    m_dset_fid_map[dset_idx++] = it->first;

  if(enableVerboseLogging()){
    std::cout << "ADExecDataInterface created with #datasets=" << this->nDataSets() << " (exec-data map size " << execDataMap->size() << ")" << " with dset_idx:fid mapping ";
    for(int d=0;d<this->nDataSets();d++) std::cout << d << ":" << m_dset_fid_map[d] << " ";
    std::cout << std::endl;
  }
}

std::pair<bool, double> ADExecDataInterface::getStatisticValue(const ExecData_t &e) const{
  switch(this->m_statistic.first){
  case None:
    return {false,0};
  case ExclusiveRuntime:
    return {true, e.get_exclusive()};
  case InclusiveRuntime:
    return {true, e.get_inclusive()};
  case Counter:
    for(auto const &c : e.get_counters())
      if(c.get_counterid() == this->m_statistic.second)
        return {true, c.get_value()};
      return {false, 0};
  default:
    throw std::runtime_error("Invalid statistic");
  }
}

bool ADExecDataInterface::ignoringFunction(const std::string &func) const{
  return m_func_ignore.count(func) != 0;
}

void ADExecDataInterface::setIgnoreFunction(const std::string &func){
  m_func_ignore.insert(func);
}

size_t ADExecDataInterface::getDataSetModelIndex(size_t dset_index) const{
  if(dset_index >= m_dset_fid_map.size()){
    std::string err = "Invalid dset_index " +std::to_string(dset_index);
    fatal_error(err);
  }
  return m_dset_fid_map[dset_index];
}

size_t ADExecDataInterface::getDataSetIndexOfFunction(size_t fid) const{
  for(size_t dset_idx = 0; dset_idx < m_dset_fid_map.size(); dset_idx++)
    if(m_dset_fid_map[dset_idx] == fid) return dset_idx;
  fatal_error("Could not find function index");
}

CallListIterator_t ADExecDataInterface::getExecDataEntry(size_t dset_index, size_t elem_index) const{
  auto it = m_execDataMap->find(m_dset_fid_map[dset_index]);
  if(it == m_execDataMap->end()){ fatal_error("Invalid dset_idx"); }
  if(elem_index >= it->second.size()){ fatal_error("Invalid elem_index"); }
  return it->second[elem_index];
}

std::vector<ADDataInterface::Elem> ADExecDataInterface::getDataSet(size_t dset_index) const{
  verboseStream << "ADExecDataInterface::getDataSet with dset_index=" << dset_index << std::endl;
  if(dset_index >= m_dset_fid_map.size()) fatal_error("Invalid dset_index " + std::to_string(dset_index));
      
  std::vector<ADDataInterface::Elem> out;  
  size_t fid = m_dset_fid_map[dset_index];
  verboseStream << "ADExecDataInterface::getDataSet found function id " << fid << std::endl;
  
  auto dit = m_execDataMap->find(fid);
  if(dit == m_execDataMap->end()){
    std::string err = "No dataset exists in execDataMap with fid=" + std::to_string(fid);    
    fatal_error(err);
  }  
  auto const &data = dit->second;
  verboseStream << "ADExecDataInterface::getDataSet data set contains " << data.size() << " entries" << std::endl;
  
  if(data.size() == 0) return out;
  const std::string &fname = data.front()->get_funcname();
  if(fname.length() > 50000){
    std::string err = "Function name is >50k characters, suggests memory corruption. 1st 100 chars: " + fname.substr(0,100);
    fatal_error(err);
  }
  
  bool ignore_func = ignoringFunction(fname);

  std::array<unsigned long, 4> fkey;

  for (size_t i = 0; i < data.size(); i++)
  { // loop over events for that function
    auto &e = *data[i];
    if (e.get_label() == 0){ // has not been analyzed previously
      if (ignore_func)
        e.set_label(1); // label as normal event
      else if (m_ignore_first_func_call && !m_local_func_exec_seen->count(fkey = {e.get_pid(), e.get_rid(), e.get_tid(), fid})){        
        e.set_label(1);
        m_local_func_exec_seen->insert(fkey);
      }else{
        auto v = getStatisticValue(e);
        if(v.first)
          out.push_back(ADDataInterface::Elem(v.second, i));
      }
    }
  }
  verboseStream << "ADExecDataInterface::getDataSet for dset_index=" << dset_index << " (fid=model_idx=" << fid << " fname=\"" << fname << "\") got " << out.size() << " data" << std::endl;
  return out;
}

void ADExecDataInterface::recordDataSetLabelsInternal(const std::vector<Elem> &data, size_t dset_index){
  auto it = m_execDataMap->find(m_dset_fid_map[dset_index]);
  if(it == m_execDataMap->end()){ fatal_error("Invalid dset_idx"); }
  for(auto const &e: data){
    if(e.index >= it->second.size()){ fatal_error("Invalid element index"); }
    CallListIterator_t eint = it->second[e.index];
    eint->set_outlier_score(e.score);
    if(e.label != EventType::Unassigned){
      eint->set_label( e.label == EventType::Outlier ? -1 : 1 );
    }
  }
}
