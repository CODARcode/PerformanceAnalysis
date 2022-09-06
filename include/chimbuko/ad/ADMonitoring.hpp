#pragma once
#include <chimbuko_config.h>
#include "chimbuko/ad/ADCounter.hpp"

namespace chimbuko {

  /**
   * @brief A class that parses the TAU monitoring plugin counters to maintain a most recent state of the process / node
   */
  class ADMonitoring{
  public:
    ADMonitoring(): m_counterMap(nullptr), m_timestamp(0), m_state_set(false){}

    /**
     * @brief Parse the counters on this timestep and extract those associated with the monitoring plugin
     */
    void extractCounters(const CountersByIndex_t &counters);

    /**
     * @brief pass in the pointer to the mapping of counter index to counter description
     * 
     * @param m hash map to counter descriptions
     */
    void linkCounterMap(const std::unordered_map<int, std::string>* m) { m_counterMap = m; }

    /**
     * @brief Return the state in JSON format
     */
    nlohmann::json get_json() const;

    /**
     * @brief A struct representing the state of a particular property
     */
    struct StateEntry{
      std::string field_name; /**< The field name in the output JSON*/
      unsigned long value; /**< The most recent value*/
      bool assigned; /**< A value has been assigned previously*/
      StateEntry(const std::string &field_name): field_name(field_name), value(0), assigned(false){}
    };

    /**
     * @brief Check if a state exists for a particular field
     */
    bool hasState(const std::string &field_name) const;

    /**
     * @brief Get the state for a particular field
     */
    const StateEntry & getState(const std::string &field_name) const;

    /**
     * @brief Add a counter to watch
     * @param counter_name The counter name as it appears in the input data stream
     * @param field_name The counter name as it is referred to for access here and in the class JSON output (user chosen)
     */
    void addWatchedCounter(const std::string &counter_name, const std::string &field_name);

    /**
     * @brief Setup the default watch list
     */
    void setDefaultWatchList();
    
    /**
     * @brief Get the timestamp of the most recent update
     */
    unsigned long getTimeStamp() const{ return m_timestamp; }

    /**
     * @brief Check if the state has been set at least once
     */
    bool stateIsSet() const{ return m_state_set; }

    /**
     * @brief Clear the counter watch list
     */
    void clearWatchList(){ m_counter_watchlist.clear(); }

    /**
     * @brief Set the watch list from the provided file
     *
     * File should be formated as JSON as an array where each element is a 2-element array containing
     * the counter name followed by the field name
     * e.g.
     * [
     *   ["my counter name", "its field name"],
     *   ["my other counter name", "this one's field name"],
     * ]
     *
     * Note: existing watch list will be cleared first
     */
    void parseWatchListFile(const std::string &file);

    /**
     * @brief Get the current watchlist
     */
    const std::unordered_map<std::string, std::string>& getWatchList() const{ return m_counter_watchlist; }

  private:
    bool m_state_set; /**< True if the state has been set at least once*/
    unsigned long m_timestamp; /**< The timestamp of the most recent update*/
    std::list<StateEntry> m_state; /**< A list of current state entries*/
    std::map<int, std::list<StateEntry>::iterator> m_cid_state_map; /**< A map of counter index to a state entry*/
    std::unordered_map<std::string, std::list<StateEntry>::iterator> m_fieldname_state_map; /**< A map of field name to a state entry (used for manual lookup)*/

    std::unordered_map<std::string, std::string> m_counter_watchlist; /**< A list of counters that we would like to capture and their associated field names in the output*/
    std::set<int> m_cid_seen; /**< A list of counter indices that have previously been checked*/

    const std::unordered_map<int, std::string> *m_counterMap; /**< counter index -> counter name map */
  
  };

}
