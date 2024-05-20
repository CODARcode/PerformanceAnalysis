#pragma once

#include<chimbuko_config.h>
#include<string>

namespace chimbuko{

  enum MessageKind {
    ANOMALY_STATS = 0,
    COUNTER_STATS = 1,
    FUNCTION_INDEX = 2,
    ANOMALY_METRICS = 3,
    AD_PS_COMBINED_STATS = 4
  };

  std::string toString(const MessageKind m);
};
