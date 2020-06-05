#pragma once
#include "chimbuko/ad/ADDefine.hpp"
#include "chimbuko/ad/ExecData.hpp"
#include <string>
#include <vector>
#include <list>
#include <stack>
#include <unordered_map>

namespace chimbuko {
  typedef std::list<CounterData_t> CounterDataList_t;
  typedef typename CounterDataList_t::iterator CounterDataListIterator_t;
  typedef std::map<unsigned long, std::list<CounterDataListIterator_t> > CounterTimeStamps_t;
  
  /**
   * @brief map of process, rank, thread -> CounterDataList_t
   */
  DEF_MAP3UL(CounterDataListMap, CounterDataList_t); 

  /**
   * @brief map of process, rank, thread -> CounterTimeStamps_t
   */
  DEF_MAP3UL(CounterTimeStampMap, CounterTimeStamps_t); 

  
  /**
   * @brief A class that stores counter events
   */
  class ADCounter{
  public:
    ADCounter(): m_counterMap(nullptr), m_counters(nullptr){}
    ~ADCounter(){ if(m_counters) delete m_counters; }
    
    /**
     * @brief pass in the pointer to the mapping of counter index to counter description
     * 
     * @param m hash map to counter descriptions
     */
    void linkCounterMap(const std::unordered_map<int, std::string>* m) { m_counterMap = m; }

    /**
     * @brief Insert a new counter
     * @param event Event_t wrapper around the counter data
     */
    void addCounter(const Event_t& event);

    /**
     * @brief Return all counters collected in the timestep
     */
    CounterDataListMap_p_t const* getCounters() const{ return m_counters; }

    /**
     * @brief Return all counters and clear internal state
     * @return A pointer to a list of counters (should be deleted externally)
     */
    CounterDataListMap_p_t* flushCounters();

    /**
     * @brief Get counters for a particular process/rank/thread that were recorded in the window (t_start, t_end) [inclusive]
     */
    std::list<CounterDataListIterator_t> getCountersInWindow(const unsigned long pid, const unsigned long rid, const unsigned long tid,
							     const unsigned long t_start, const unsigned long t_end) const;

    
  private:
    CounterDataListMap_p_t* m_counters; /**< process/rank/thread -> List of counters */
    const std::unordered_map<int, std::string> *m_counterMap; /**< counter index -> counter name map */
    CounterTimeStampMap_p_t m_timestampCounterMap; /** < process/rank/thread -> *Ordered* map of timestamp to counter list iterator (flushed with flushCounters) */
  };
};
