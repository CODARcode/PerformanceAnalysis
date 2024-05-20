#include<chimbuko/modules/performance_analysis/pserver/PScommon.hpp>

using namespace chimbuko;

std::string chimbuko::toString(const MessageKind m){
#define KSTR(A) case A: return #A

  switch(m){
    KSTR(ANOMALY_STATS);
    KSTR(COUNTER_STATS);
    KSTR(FUNCTION_INDEX);
    KSTR(ANOMALY_METRICS);
    KSTR(AD_PS_COMBINED_STATS);
    default: return "UNKNOWN";
  }
#undef KSTR
}
