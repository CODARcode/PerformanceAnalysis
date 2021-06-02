#include<chimbuko/param/sstd_param.hpp>
#include "id_map.hpp"

namespace chimbuko_sim{
  using namespace chimbuko;

  inline SstdParam & globalParams(){ static SstdParam p; return p; }
  
  //"register" a function, generating fake statistics given a provided mean and standard deviation
  void registerFunc(const std::string &func_name, unsigned long mean_runtime, unsigned long std_dev_runtime, int seen_count);
};
