#pragma once

#include <chimbuko/ad/ADEvent.hpp>
#include <chimbuko/param.hpp>
#include "chimbuko/ad/ADCounter.hpp"

namespace chimbuko{

  /**
   * @brief A class that gathers provenance data associated with a detected anomaly
   */
  class ADAnomalyProvenance{
  public:
    ADAnomalyProvenance(const ExecData_t &call,
			const ADEvent &event_man, //for stack trace
			const ParamInterface &func_stats, //for func stats
			const ADCounter &counters //for counters
			);

    /**
     * @brief Serialize anomaly data into JSON construct
     */
    nlohmann::json get_json() const;
    
  private:
    ExecData_t m_call; /**< The anomalous event*/
    std::vector<nlohmann::json> m_callstack; /**< Call stack from function back to root. Each entry is the function index and name */
    nlohmann::json m_func_stats; /**< JSON object containing run statistics of the anomalous function */
    std::vector<nlohmann::json> m_counters; /**< A list of counter events that occurred during the execution of the anomalous function*/
  };


};
