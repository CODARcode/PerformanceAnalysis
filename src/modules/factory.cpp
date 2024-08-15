#include<chimbuko/modules/factory.hpp>
#include<chimbuko/modules/performance_analysis/chimbuko.hpp>
#include<chimbuko/modules/performance_analysis/provdb/ProvDBmoduleSetup.hpp>
#include<chimbuko/modules/performance_analysis/pserver/PSmoduleDataManager.hpp>
#include<chimbuko/modules/performance_analysis/provdb/ProvDBprune.hpp>
#include<chimbuko/core/util/error.hpp>

std::unique_ptr<chimbuko::ChimbukoBase> chimbuko::modules::factoryInstantiateChimbuko(const std::string &module, int argc, char** argv){
  if(module == "performance_analysis"){    
    return performance_analysis::moduleInstantiateChimbuko(argc,argv);
  }else{
    fatal_error("Unknown module");
  }
}

std::unique_ptr<chimbuko::ProvDBmoduleSetupCore> chimbuko::modules::factoryInstantiateProvDBmoduleSetup(const std::string &module){
  if(module == "performance_analysis"){
    return std::unique_ptr<ProvDBmoduleSetupCore>(new performance_analysis::ProvDBmoduleSetup);
  }else{
    fatal_error("Unknown module");
  }
}

std::unique_ptr<chimbuko::PSmoduleDataManagerCore> chimbuko::modules::factoryInstantiatePSmoduleDataManager(const std::string &module, int net_nworker){
  if(module == "performance_analysis"){
    return std::unique_ptr<PSmoduleDataManagerCore>(new performance_analysis::PSmoduleDataManager(net_nworker));
  }else{
    fatal_error("Unknown module");
  }
}

std::unique_ptr<chimbuko::ProvDBpruneCore> chimbuko::modules::factoryInstantiateProvDBprune(const std::string &module){
  if(module == "performance_analysis"){
    return std::unique_ptr<ProvDBpruneCore>(new performance_analysis::ProvDBprune);
  }else{
    fatal_error("Unknown module");
  }
}


std::vector<std::string> chimbuko::modules::factoryListModules(){
  return { "performance_analysis" };
};
