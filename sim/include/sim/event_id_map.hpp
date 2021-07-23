#include <chimbuko_config.h>
#include<chimbuko/ad/ADEvent.hpp>

namespace chimbuko_sim{
  using namespace chimbuko;

  /**
   * @brief An implementation of ADEventIDmap interface for the simulator using external maps
   */
  class eventIDmap: public ADEventIDmap{
    const std::unordered_map<unsigned long, std::list<ExecData_t> > &m_thread_exec_map;
    const std::unordered_map<eventID, CallListIterator_t> &m_eventid_map;

  public:
    eventIDmap(const std::unordered_map<unsigned long, std::list<ExecData_t> > &thread_exec_map, 
	       const std::unordered_map<eventID, CallListIterator_t> &eventid_map): m_thread_exec_map(thread_exec_map), m_eventid_map(eventid_map){}
  
    CallListIterator_t getCallData(const eventID &event_id) const override;

    std::pair<CallListIterator_t, CallListIterator_t> getCallWindowStartEnd(const eventID &event_id, const int win_size) const override;
  };

};
