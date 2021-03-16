#include "chimbuko/param.hpp"
#include <chrono>

using namespace chimbuko;

ParamInterface::ParamInterface()
{

}

ParamInterface *ParamInterface::set_AdParam(const std::string & ad_algorithm) {
  //std::string hbos = 'hbos', sstd = 'sstd';
  switch(ad_algorithm) {
    case 'hbos': return new HbosParam();
    case 'sstd': return new SstdParam();
    default:return nullptr;
  }
  //
  // if (ad_algorithm.compare(hbos) == 0) {
  //   return new HbosParam();
  // }
  // else if (ad_algorithm.compare(sstd) == 0) {
  //   return new SstdParam();
  // }
  // else {
  //   return nullptr;
  // }
}
