#include<chimbuko/modules/factory.hpp>
#include<chimbuko/modules/performance_analysis/chimbuko.hpp>
#include<chimbuko/core/util/error.hpp>

using namespace chimbuko;

std::unique_ptr<ChimbukoBase> chimbuko::factoryInstantiateChimbuko(const std::string &module, int argc, char** argv){
  if(module == "performance_analysis"){    
    return moduleInstantiateChimbuko(argc,argv);
  }else{
    fatal_error("Unknown module");
  }
}

