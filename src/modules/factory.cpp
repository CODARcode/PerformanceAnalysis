#include<chimbuko/modules/factory.hpp>
#include<chimbuko/modules/performance_analysis/chimbuko.hpp>
#include<chimbuko/modules/performance_analysis/provdb/ProvDBmoduleSetup.hpp>
#include<chimbuko/core/util/error.hpp>

using namespace chimbuko;

std::unique_ptr<ChimbukoBase> chimbuko::factoryInstantiateChimbuko(const std::string &module, int argc, char** argv){
  if(module == "performance_analysis"){    
    return moduleInstantiateChimbuko(argc,argv);
  }else{
    fatal_error("Unknown module");
  }
}

std::unique_ptr<ProvDBmoduleSetupCore> chimbuko::factoryInstantiateProvDBmoduleSetup(const std::string &module){
  if(module == "performance_analysis"){
    return std::unique_ptr<ProvDBmoduleSetupCore>(new ProvDBmoduleSetup);
  }else{
    fatal_error("Unknown module");
  }
}
