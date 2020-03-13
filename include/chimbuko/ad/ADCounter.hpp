#pragma once
#include "chimbuko/ad/ADDefine.hpp"
#include "chimbuko/ad/ExecData.hpp"
#include <string>
#include <vector>
#include <list>
#include <stack>
#include <unordered_map>

namespace chimbuko {
  typedef std::list<CounterData_t> CounterDataList;

  /**
   * @brief A class that stores counter events
   */
  class ADCounters{
  public:
    ADCounters(): m_counterMap(nullptr), m_counters(nullptr){}
    ~ADCounters(){ if(m_counters) delete m_counters; }
    
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
     * @brief Return all counters and clear internal state
     * @return A pointer to a list of counters (should be deleted externally)
     */
    CounterDataList* flushCounters();
    

  private:
    CounterDataList* m_counters;
    const std::unordered_map<int, std::string> *m_counterMap;
    
  };
};
