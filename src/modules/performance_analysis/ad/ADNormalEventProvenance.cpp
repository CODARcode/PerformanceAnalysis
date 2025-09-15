#include<chimbuko/modules/performance_analysis/ad/ADNormalEventProvenance.hpp>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

void ADNormalEventProvenance::addNormalEvent(const unsigned long pid, const unsigned long rid, const unsigned long tid, const unsigned long fid, const nlohmann::json &event){
  m_normal_events[pid][rid][tid][fid] = event;
}

void ADNormalEventProvenance::addOutstanding(const unsigned long pid, const unsigned long rid, const unsigned long tid, const unsigned long fid){
  m_outstanding_normal_events[pid][rid][tid].insert(fid);
}

std::pair<nlohmann::json, bool> ADNormalEventProvenance::getNormalEvent(const unsigned long pid, const unsigned long rid, const unsigned long tid, const unsigned long fid, bool add_outstanding, bool do_delete){
  std::pair<nlohmann::json, bool> out = { nlohmann::json({}), false };

  auto pit = m_normal_events.find(pid);
  if(pit == m_normal_events.end()){ 
    if(add_outstanding) 
      addOutstanding(pid,rid,tid,fid); 
    return out; 
  }

  auto rit = pit->second.find(rid);
  if(rit == pit->second.end()){ 
    if(add_outstanding) 
      addOutstanding(pid,rid,tid,fid); 
    return out; 
  }

  auto tit = rit->second.find(tid);
  if(tit == rit->second.end()){ 
    if(add_outstanding)
      addOutstanding(pid,rid,tid,fid); 
    return out; 
  }

  auto fit = tit->second.find(fid);
  if(fit == tit->second.end()){ 
    if(add_outstanding)
      addOutstanding(pid,rid,tid,fid); 
    return out; 
  }
  
  out.first = fit->second;
  out.second = true;

  if(do_delete) tit->second.erase(fit); //remove the entry to prevent it being used again

  return out;
}


std::vector<nlohmann::json> ADNormalEventProvenance::getOutstandingRequests(bool do_delete){
  std::vector<nlohmann::json> out;
  //oe = outstanding event request maps
  //ne = normal event data maps
  //*it indicates an iterator

  for(auto &oe_p : m_outstanding_normal_events){
    auto ne_pit = m_normal_events.find(oe_p.first);
    if(ne_pit == m_normal_events.end()) continue; //no normal events with this pid

    for(auto &oe_r : oe_p.second){
      auto ne_rit = ne_pit->second.find(oe_r.first);
      if(ne_rit == ne_pit->second.end()) continue; //no normal events with this rid

      for(auto &oe_t : oe_r.second){
	auto ne_tit = ne_rit->second.find(oe_t.first);
	if(ne_tit == ne_rit->second.end()) continue;  //no normal events with this tid
	
	//Loop over all outstanding functions, if can satisfy remove from list
	auto oe_fit = oe_t.second.begin(); 
	while(oe_fit != oe_t.second.end()){
	  auto ne_fit = ne_tit->second.find(*oe_fit);
	  if(ne_fit == ne_tit->second.end()){ //no normal event with this fid
	    ++oe_fit;
	  }else{
	    const nlohmann::json &nev = ne_fit->second; //the normal event
	    out.push_back(nev);
	    oe_fit = oe_t.second.erase(oe_fit); //remove now-satisfied request
	    if(do_delete) ne_tit->second.erase(ne_fit); //remove now used normal event
	  }
	}
      }
    }
  }
  return out;
}


