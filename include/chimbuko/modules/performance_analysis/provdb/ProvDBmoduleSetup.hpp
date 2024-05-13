#pragma once
#include <chimbuko_config.h>
#include <chimbuko/core/provdb/ProvDBmoduleSetupCore.hpp>

namespace chimbuko {

  class ProvDBmoduleSetup: public ProvDBmoduleSetupCore{
  public:
    /**
     * @brief Return an array of collection names for those associated with provenance data provided by the AD clients
     */
    std::vector<std::string> getMainDBcollections() const override{ return {"anomalies", "metadata", "normalexecs"}; }


    /**
     * @brief Return an array of collection names for those associated with provenance data provided by the parameter server (i.e. the "global" database)
     */
    std::vector<std::string> getGlobalDBcollections() const override{ return {"func_stats", "counter_stats", "ad_model"}; }

  };

};
