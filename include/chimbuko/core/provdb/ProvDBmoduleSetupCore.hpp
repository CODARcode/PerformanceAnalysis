#pragma once
#include <chimbuko_config.h>
#include <vector>
#include <string>

namespace chimbuko {

  class ProvDBmoduleSetupCore{
  public:
    /**
     * @brief Return an array of collection names for those associated with provenance data provided by the AD clients
     */
    virtual std::vector<std::string> getMainDBcollections() const = 0;

    /**
     * @brief Return an array of collection names for those associated with provenance data provided by the parameter server (i.e. the "global" database)
     */
    virtual std::vector<std::string> getGlobalDBcollections() const = 0;

    virtual ~ProvDBmoduleSetupCore(){}
  };

};
