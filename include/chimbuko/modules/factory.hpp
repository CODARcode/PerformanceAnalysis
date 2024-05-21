#pragma once
#include<chimbuko_config.h>
#include<chimbuko/core/chimbuko.hpp>

namespace chimbuko{
  /**
   *@brief A factory function for Chimbuko (OAD) instances
   */
  std::unique_ptr<ChimbukoBase> factoryInstantiateChimbuko(const std::string &module, int argc, char** argv);
}
