#include "chimbuko/core/param.hpp"
#include "chimbuko/core/param/sstd_param.hpp"
#include "chimbuko/core/param/hbos_param.hpp"
#include "chimbuko/core/param/copod_param.hpp"
#include "chimbuko/util/error.hpp"
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
    fatal_error("Invalid algorithm: \"" + ad_algorithm + "\". Available options: HBOS, SSTD, COPOD");
  }
}

void ParamInterface::update(const std::vector<ParamInterface*> &other){
  for(auto p : other){
    this->update(*p);
  }
}
