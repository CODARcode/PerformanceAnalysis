#include "chimbuko/param.hpp"
#include <chrono>

using namespace chimbuko;

ParamInterface::ParamInterface()
{

}

ParamInterface *ParamInterface::set_AdParam(const std::string & ad_algorithm) {
  std::string hbos = "hbos", sstd = "sstd";
  if (ad_algorithm == hbos) {
    HbosParam *param = new HbosParam();
    return param; // new HbosParam();
  }
  else if (ad_algorithm == sstd) {
    return new SstdParam();
  }
  else {
    return nullptr;
  }
}
