#pragma once

#include<chimbuko/pserver/global_anomaly_stats.hpp>
#include<chimbuko/pserver/global_counter_stats.hpp>

namespace chimbuko_sim{
  using namespace chimbuko;

  //An object that represents the pserver for the purposes of aggregating data over AD instances and writing streaming output to disk
  class pserverSim{
    GlobalAnomalyStats global_func_stats; //global anomaly statistics
    GlobalCounterStats global_counter_stats; //global counter statistics
    PSstatSenderGlobalAnomalyStatsPayload anomaly_stats_payload;
    PSstatSenderGlobalCounterStatsPayload counter_stats_payload;
  public:

    pserverSim(): anomaly_stats_payload(&global_func_stats), counter_stats_payload(&global_counter_stats){}
  
    //Stand-in for the func statistics comms from AD -> pserver
    void addAnomalyData(const ADLocalFuncStatistics &loc){
      global_func_stats.add_anomaly_data(loc);
    }
    void addCounterData(const ADLocalCounterStatistics &loc){
      global_counter_stats.add_counter_data(loc);
    }

    //Mirror write functionality of PSstatSender
    void writeStreamingOutput() const;
  };  


  inline pserverSim & getPserver(){ static pserverSim ps; return ps; }

};
