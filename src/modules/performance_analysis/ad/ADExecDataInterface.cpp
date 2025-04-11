#include <chimbuko/modules/performance_analysis/ad/ADExecDataInterface.hpp>
#include <chimbuko/core/verbose.hpp>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

ADExecDataInterface::ADExecDataInterface(ExecDataMap_t const* execDataMap, OutlierStatistic stat): m_execDataMap(execDataMap), m_statistic(stat), ADDataInterface(execDataMap->size()), m_dset_fid_map(execDataMap->size()), m_ignore_first_func_call(false){
  //Build a map between a data set index and the function indices
  size_t dset_idx = 0;
  for(auto it = execDataMap->begin(); it != execDataMap->end(); ++it)
    m_dset_fid_map[dset_idx++] = it->first;
}

double ADExecDataInterface::getStatisticValue(const ExecData_t &e) const{
  switch(m_statistic){
  case ExclusiveRuntime:
    return e.get_exclusive();
  case InclusiveRuntime:
    return e.get_inclusive();
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

size_t ADExecDataInterface::getDataSetIndexOfFunction(size_t fid) const{
  for(size_t dset_idx = 0; dset_idx < m_dset_fid_map.size(); dset_idx++)
    if(m_dset_fid_map[dset_idx] == fid) return dset_idx;
  fatal_error("Could not find function index");
}

CallListIterator_t ADExecDataInterface::getExecDataEntry(size_t dset_index, size_t elem_index) const{
  auto it = m_execDataMap->find(m_dset_fid_map[dset_index]);
  if(it == m_execDataMap->end()){ fatal_error("Invalid dset_idx"); }
  return it->second[elem_index];
}

std::vector<ADDataInterface::Elem> ADExecDataInterface::getDataSet(size_t dset_index) const{
  auto it = m_dset_cache.find(dset_index);
  if(it != m_dset_cache.end()) return it->second;

  std::vector<ADDataInterface::Elem> out;  
  size_t fid = m_dset_fid_map[dset_index];
  auto const &data = m_execDataMap->find(fid)->second;

  if(data.size() == 0) return out;
  const std::string &fname = data.front()->get_funcname();
  bool ignore_func = ignoringFunction(fname);

  std::array<unsigned long, 4> fkey;

  for(size_t i=0;i<data.size();i++){ //loop over events for that function
    auto &e = *data[i];
    if(e.get_label() == 0){ //has not been analyzed previously
      if(ignore_func) e.set_label(1); //label as normal event
      else if(m_ignore_first_func_call && !m_local_func_exec_seen->count(fkey = {e.get_pid(), e.get_rid(), e.get_tid(), fid}) ){
	//Note, because we cache the data sets, successive calls to getDataSet with the same dset_index will always return the same thing despite marking this function as seen
	e.set_label(1);
	m_local_func_exec_seen->insert(fkey);
      }else{
	out.push_back(ADDataInterface::Elem(getStatisticValue(e),i));
      }
    }
  }
  verboseStream << "ADExecDataInterface::getDataSet for dset_index=" << dset_index << " (fid=model_idx=" << fid << " fname=\"" << fname << "\") got " << out.size() << " data" << std::endl;
  m_dset_cache[dset_index] = out; //store *unlabeled* data
  return out;
}

void ADExecDataInterface::recordDataSetLabelsInternal(const std::vector<Elem> &data, size_t dset_index){
  auto it = m_execDataMap->find(m_dset_fid_map[dset_index]);
  if(it == m_execDataMap->end()){ fatal_error("Invalid dset_idx"); }
  for(auto const &e: data){
    CallListIterator_t eint = it->second[e.index];
    eint->set_outlier_score(e.score);
    if(e.label == EventType::Unassigned){ fatal_error("Encountered an unassigned label when recording"); }
    eint->set_label( e.label == EventType::Outlier ? -1 : 1 );
  }
}
