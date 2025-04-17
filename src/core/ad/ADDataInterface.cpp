#include <chimbuko/core/ad/ADDataInterface.hpp>
#include <chimbuko/core/util/error.hpp>

using namespace chimbuko;

void ADDataInterface::DataSetAnomalies::recordEvent(const Elem &event){
  if(event.label == EventType::Normal){
    //Store a max of one normal event. Select that with the lower score (more normal)
    if(m_normal_events.size() == 0) m_normal_events.push_back(event);    
    else if(event.score < m_normal_events[0].score) m_normal_events[0] = event;
    ++m_labeled_events; //increment even if not recorded
  }else if(event.label == EventType::Outlier){ //Outlier
    m_anomalies.push_back(event);
    ++m_labeled_events; //increment even if not recorded
  }
}

size_t ADDataInterface::nEventsRecorded(EventType type) const{
  size_t out = 0;
  for(size_t i=0;i<this->nDataSets();i++)
    out += this->getResults(i).nEventsRecorded(type);
  return out;
}

size_t ADDataInterface::nEvents() const{
  size_t out = 0;
  for(size_t i=0;i<this->nDataSets();i++)
    out += this->getResults(i).nEvents();
  return out;
}

void ADDataInterface::recordDataSetLabels(const std::vector<Elem> &data, size_t dset_index){
  auto &danom = m_dset_anom[dset_index];
  for(auto const &e : data) danom.recordEvent(e);
  this->recordDataSetLabelsInternal(data,dset_index);
}

