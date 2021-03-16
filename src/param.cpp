#include "chimbuko/param.hpp"
#include <chrono>

using namespace chimbuko;

ParamInterface::ParamInterface()
{

}

ParamInterface *ParamInterface::set_AdParam(const std::string & ad_algorithm) {
  if (!ad_algorithm.compare('hbos') || !ad_algorithm.compare('HBOS'))
    return new HbosParam();
  else if (!ad_algorithm.compare('sstd') || !ad_algorithm.compare('SSTD'))
    return new SstdParam();
  else
    return nullptr;
}
