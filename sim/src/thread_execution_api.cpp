#include<sim/thread_execution_api.hpp>

using namespace chimbuko;
using namespace chimbuko_sim;

void threadExecution::enterNormal(const std::string &func, const long ts, const std::string &tag){
  assert(ts >= m_tlast);
  m_stack.push(openFunction(func, ts, tag, false, 0));
  m_tlast = ts;
}
void threadExecution::enterAnomaly(const std::string &func, const long ts, double score, const std::string &tag){
  assert(ts >= m_tlast);
  m_stack.push(openFunction(func, ts, tag, true, score));
  m_tlast = ts;
}

void threadExecution::addCounter(const std::string &name, const unsigned long value, const long ts){
  assert(ts >= m_tlast);
  long t_delta = ts - m_stack.top().entry;
  assert(t_delta >= 0);
  m_stack.top().counters.push_back(counter(name, value, t_delta));
  m_tlast = ts;
}
 
void threadExecution::addComm(CommType type, unsigned long partner_rank, unsigned long bytes, long ts){
  assert(ts >= m_tlast);
  long t_delta = ts - m_stack.top().entry;
  assert(t_delta >= 0);
  m_stack.top().comms.push_back(comm(type, partner_rank, bytes, t_delta));
  m_tlast = ts;
}


void threadExecution::exit(const long ts){
  assert(ts >= m_tlast);

  const openFunction &f = m_stack.top();
  CallListIterator_t it = m_ad.addExec(m_tid, f.func, f.entry, ts - f.entry, f.is_anomaly, f.score);
  if(f.tag != "") m_tagged_events[f.tag] = it;

  //Deal with children
  for(auto const &c : f.children){
    it->inc_n_children();
    it->update_exclusive(c->get_runtime());
    //Tell the child who it's parent is
    c->set_parent(it->get_id());
  }

  //Deal with comms and counters
  for(auto const &cn: f.counters)
    m_ad.attachCounter(cn.name, cn.value, cn.t_delta, it);

  for(auto const &cm: f.comms)
    m_ad.attachComm(cm.type, cm.partner_rank, cm.bytes, cm.t_delta, it);

  m_stack.pop();
  if(m_stack.size()) m_stack.top().children.push_back(it);
  m_tlast = ts;
}

CallListIterator_t threadExecution::getTagged(const std::string &tag) const{
  auto it = m_tagged_events.find(tag);
  assert(it != m_tagged_events.end());
  return it->second;
}    
