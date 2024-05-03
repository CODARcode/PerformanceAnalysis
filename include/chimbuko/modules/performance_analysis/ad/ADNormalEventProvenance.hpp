#pragma once
#include <chimbuko_config.h>
#include<array>
#include<unordered_set>
#include<unordered_map>
#include <nlohmann/json.hpp>

namespace chimbuko {  
  /**
   * @brief A class that maintains the provenance information for the most recent normal event for each encountered function
   * A mechanism is provided for dealing with cases where a normal execution is not yet available
   * Once returned the internal copy is deleted ensuring a given normal execution is only ever output once
   */
  class ADNormalEventProvenance {
  public:
    /**
     * @brief Add a normal event. If a normal event already exists with this pid,rid,tid,fid it will be overwritten
     */
    void addNormalEvent(const unsigned long pid, const unsigned long rid, const unsigned long tid, const unsigned long fid, const nlohmann::json &event);
    
    /**
     * @brief Get a normal event if available
     * @param add_outstanding If true and the event is not available the pid/rid/tid/fid will be placed in a list of outstanding requests that will be furnished later
     * @param do_delete If true and the event is available, the stored copy will be deleted
     * @return The JSON data if available, and a bool indicating if the data is populated
     */
    std::pair<nlohmann::json, bool> getNormalEvent(const unsigned long pid, const unsigned long rid, const unsigned long tid, const unsigned long fid, bool add_outstanding, bool do_delete);

    /**
     * @brief For normal event requests that were not previously available, calls to this function will see if a normal event now exists
     * @param do_delete If true and the event is available, the stored copy will be deleted
     * @return A vector of outstanding requests that have now been furnished
     */
    std::vector<nlohmann::json> getOutstandingRequests(bool do_delete);

  private:
    /**
     * @brief Add an entry to the list of outstanding requests
     */
    void addOutstanding(const unsigned long pid, const unsigned long rid, const unsigned long tid, const unsigned long fid);

    std::unordered_map<unsigned long,
		       std::unordered_map<unsigned long,
					  std::unordered_map<unsigned long,
							     std::unordered_map<unsigned long, nlohmann::json>
							     >
					  >
		       > m_normal_events; /**< Map of process/rank/thread/function_idx -> provenance of the most recent normal event */

    std::unordered_map<unsigned long,
		       std::unordered_map<unsigned long,
					  std::unordered_map<unsigned long,
							     std::unordered_set<unsigned long>
							     >
					  >
		       > m_outstanding_normal_events; /**< Map of process/rank/thread -> set of function_idx for which we were unable to previously provide a normal event */
      
  };
}
