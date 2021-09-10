#pragma once
#include <chimbuko_config.h>
#include <chimbuko/pserver/GlobalAnomalyStats.hpp>
#include <chimbuko/pserver/GlobalCounterStats.hpp>

namespace chimbuko{
  
    /**
   * @brief Net payload for receiving the combined data payload from the AD
   */
  class NetPayloadRecvCombinedADdata: public NetPayloadBase{
    GlobalAnomalyStats * m_global_anom_stats;
    GlobalCounterStats * m_global_counter_stats;
  public:
    NetPayloadRecvCombinedADdata(GlobalAnomalyStats * global_anom_stats, GlobalCounterStats * global_counter_stats): m_global_anom_stats(global_anom_stats),
														     m_global_counter_stats(global_counter_stats){}
    MessageKind kind() const override{ return MessageKind::AD_PS_COMBINED_STATS; }
    MessageType type() const override{ return MessageType::REQ_ADD; }
    void action(Message &response, const Message &message) override;
  };





};

