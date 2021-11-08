#include "chimbuko/param.hpp"
#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/param/hbos_param.hpp"
#include "chimbuko/param/copod_param.hpp"
#include <chrono>

using namespace chimbuko;

ParamInterface::ParamInterface()
{
}

ParamInterface *ParamInterface::set_AdParam(const std::string & ad_algorithm) {

  if (ad_algorithm == "hbos" || ad_algorithm == "HBOS") {
    return new HbosParam();
  }
  else if (ad_algorithm == "sstd" || ad_algorithm == "SSTD") {
    return new SstdParam();
  }
  else if (ad_algorithm == "copod" || ad_algorithm == "COPOD") {
    return new CopodParam();
  }
  else {
    return nullptr;
  }
}
