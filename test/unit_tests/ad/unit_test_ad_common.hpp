#pragma once
#include <chimbuko_config.h>
#include<chimbuko/ad/ADOutlier.hpp>
#include<chimbuko/param/sstd_param.hpp>
#include<chimbuko/param/hbos_param.hpp>
#include<chimbuko/param/copod_param.hpp>

namespace chimbuko{
  
  /**
   * @brief Some convenience functionality for testing and accessing protected members
   */
  template<typename Base>
  class ADOutlierCommonTest: public Base{
  public:
    ADOutlierCommonTest(): Base(0){}

    std::pair<size_t, size_t> sync_param_test(ParamInterface* param){ return this->Base::sync_param(param); }

    unsigned long compute_outliers_test(const unsigned long func_id, std::vector<CallListIterator_t>& data){
      ExecDataMap_t execdata;
      execdata[func_id] = data;
    
      ADExecDataInterface iface(&execdata);
      auto dset = iface.getDataSet(0);
      this->labelData(dset,0,func_id);
      iface.recordDataSetLabels(dset,0);

      return iface.nEventsRecorded(ADDataInterface::EventType::Outlier);
    }
    
    unsigned long compute_outliers_test(ADExecDataInterface &iface, const unsigned long func_id){
      auto dset = iface.getDataSet(0);
      this->labelData(dset,0,func_id);
      iface.recordDataSetLabels(dset,0);
      return iface.nEventsRecorded(ADDataInterface::EventType::Outlier);
    }

    static void printOutliers(const ADExecDataInterface &iface){
      for(int dset_idx=0;dset_idx<iface.nDataSets();dset_idx++){
	auto const &outliers = iface.getResults(dset_idx).getEventsRecorded(ADDataInterface::EventType::Outlier);
	int i=0;
	for(auto const &e : outliers){
	  auto c = iface.getExecDataEntry(dset_idx,e.index);
	  std::cout << dset_idx << " " << (i++) << " : " << c->get_json().dump(1);
	}
      }
    }

    static bool findOutlier(double outlier_start, double outlier_runtime, size_t dset_idx, const ADExecDataInterface &iface){    
      auto const &outliers = iface.getResults(dset_idx).getEventsRecorded(ADDataInterface::EventType::Outlier);
      for(auto const &e : outliers){
	auto c = iface.getExecDataEntry(dset_idx,e.index);
	if(c->get_entry() == outlier_start && c->get_runtime() == outlier_runtime)
	  return true;
      }
      return false;
    }

    static std::vector<CallListIterator_t> getEvents(size_t func_id, ADDataInterface::EventType type, const ADExecDataInterface &iface){
      bool found = false;
      size_t dset_idx;
      for(size_t dd=0;dd<iface.nDataSets();dd++) 
	if(iface.getDataSetParamIndex(dd) == func_id){
	  dset_idx = dd;
	  found = true;
	  break;
	}
      if(!found) fatal_error("Couldn't find func_id");
    
      auto const &r = iface.getResults(dset_idx).getEventsRecorded(type);
      std::vector<CallListIterator_t> out;
      for(auto const &e : r) out.push_back(iface.getExecDataEntry(dset_idx,e.index));
      return out;
    }
    

  };

  class ADOutlierSSTDTest: public ADOutlierCommonTest<ADOutlierSSTD>{
  public:
    double computeScoreTest(CallListIterator_t ev, const SstdParam &stats) const{
      return this->computeScore(ev->get_exclusive(),ev->get_fid(),stats);
    }
  };

  class ADOutlierHBOSTest: public ADOutlierCommonTest<ADOutlierHBOS>{
  public:

    void setParams(const HbosParam &p){ 
      ( (HbosParam*)m_param )->copy(p);
    }
    
    HbosParam & get_local_parameters() const{ return *((HbosParam*)m_local_param); }
    
    void test_updateGlobalModel(){ this->updateGlobalModel(); }
  };

  class ADOutlierCOPODTest: public ADOutlierCommonTest<ADOutlierCOPOD>{
  public:
    void setParams(const CopodParam &p){ 
      ( (CopodParam*)m_param )->copy(p);
    }
  };


  void setDataLabels(ADExecDataInterface &iface, const std::unordered_map<eventID, int> &labels){
    for(size_t d=0;d<iface.nDataSets();d++){
      auto dset = iface.getDataSet(d);
      for(auto &e : dset){
	auto it = labels.find(iface.getExecDataEntry(d,e.index)->get_id());
	assert(it != labels.end());
	int label = it->second;
	e.label = label == -1 ? ADDataInterface::EventType::Outlier : ADDataInterface::EventType::Normal;
	e.score = label == -1 ? 100 : 0;
      }
      iface.recordDataSetLabels(dset,d);
    }
  }


};
