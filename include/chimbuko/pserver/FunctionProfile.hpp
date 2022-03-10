#pragma once

#include<chimbuko_config.h>

#include<chimbuko/pserver/AggregateFuncStats.hpp>
#include<chimbuko/pserver/AggregateFuncAnomalyMetricsAllRanks.hpp>

namespace chimbuko{

  class FunctionProfile{
  private:
    typedef std::unordered_map<int, std::unordered_map<int, nlohmann::json> > AnomalyMetricsMapType; /**< Map of program idx and function idx to the profile in JSON form */
    AnomalyMetricsMapType m_profile; /**< The profile data*/

    nlohmann::json & getFunction(const int pid, const int fid, const std::string &func);
  
  public:

    /**
     * @brief Add the function runtime statistics data
     */
    void add(const AggregateFuncStats &stats);
    
    /**
     * @brief Add the function anomaly metrics data
     */
    void add(const AggregateFuncAnomalyMetricsAllRanks &metrics);

    /**
     * @brief Return the profile in JSON format
     */
    nlohmann::json get_json() const;
  };





};
