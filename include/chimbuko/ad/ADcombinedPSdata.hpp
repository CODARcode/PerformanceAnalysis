#pragma once
#include <chimbuko_config.h>
#include <chimbuko/ad/ADLocalFuncStatistics.hpp>
#include <chimbuko/ad/ADLocalCounterStatistics.hpp>
#include <chimbuko/ad/ADLocalAnomalyMetrics.hpp>

namespace chimbuko{
  /** 
   * @brief A class that combines all data that is sent to the PS into a single send transmission
   */
  class ADcombinedPSdata{
  public:
    ADcombinedPSdata(ADLocalFuncStatistics &func_stats, 
		     ADLocalCounterStatistics &counter_stats, 
		     ADLocalAnomalyMetrics &anom_metrics,
		     PerfStats* perf=nullptr): 
      m_func_stats(func_stats), m_counter_stats(counter_stats), m_anom_metrics(anom_metrics), m_perf(perf){}
    
    ADLocalCounterStatistics &get_counter_stats(){ return m_counter_stats; }
    const ADLocalCounterStatistics &get_counter_stats() const{ return m_counter_stats; }

    ADLocalFuncStatistics &get_func_stats(){ return m_func_stats; }
    const ADLocalFuncStatistics &get_func_stats() const{ return m_func_stats; }

    ADLocalAnomalyMetrics &get_anom_metrics(){ return m_anom_metrics; }
    const ADLocalAnomalyMetrics &get_anom_metrics() const{ return m_anom_metrics; }


    /**
     * @brief The State object is what is serialized for the communication
     */
    struct State{
      ADLocalFuncStatistics::State func_stats_state;
      ADLocalCounterStatistics::State counter_stats_state;
      ADLocalAnomalyMetrics::State anom_metrics_state;

      /**
       * @brief Serialize using cereal
       */
      template<class Archive>
      void serialize(Archive & archive){
	archive(func_stats_state , counter_stats_state, anom_metrics_state);
      }

      /**
       * Serialize into Cereal portable binary format
       */
      std::string serialize_cerealpb() const;
      
      /**
       * Serialize from Cereal portable binary format
       */     
      void deserialize_cerealpb(const std::string &strstate);
    };

    /**
     * @brief Get the current state as a state object
     *
     * The string dump of this object is the serialized form sent to the parameter server
     */    
    State get_state() const;


    /**
     * @brief Set the internal variables to the given state object
     */
    void set_state(const State &s);


    /**
     * @brief Serialize this class for communication over the network
     */
    std::string net_serialize() const;

    /**
     * @brief Unserialize this class after communication over the network
     */
    void net_deserialize(const std::string &s);
      
      
    /**
     * @brief Send the data to the pserver
     * @param net_client The network client object
     * @return std::pair<size_t, size_t> [sent, recv] message size
     */
    std::pair<size_t, size_t> send(ADNetClient &net_client) const;
  

  private:
    ADLocalFuncStatistics &m_func_stats;
    ADLocalCounterStatistics &m_counter_stats;
    ADLocalAnomalyMetrics &m_anom_metrics;
    PerfStats* m_perf;
  };


}
