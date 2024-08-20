#pragma once
#include<chimbuko_config.h>
#include<chimbuko/core/chimbuko.hpp>
#include<chimbuko/core/provdb/ProvDBmoduleSetupCore.hpp>
#include<chimbuko/core/provdb/ProvDBpruneCore.hpp>
#include<chimbuko/core/pserver/PSmoduleDataManagerCore.hpp>

namespace chimbuko{
  namespace modules{
    /**
     *@brief A factory function for Chimbuko (OAD) instances
     */
    std::unique_ptr<ChimbukoBase> factoryInstantiateChimbuko(const std::string &module, int argc, char** argv);

    /**
     *@brief A factory function for ProvDBmoduleSetup instances
     */
    std::unique_ptr<ProvDBmoduleSetupCore> factoryInstantiateProvDBmoduleSetup(const std::string &module);

    /**
     *@brief A factory function for ProvDBpruneCore instances
     *@param algo_params Parameters for the algoritm
     *@param model_ser The serialized model
     */   
    std::unique_ptr<ProvDBpruneCore> factoryInstantiateProvDBprune(const std::string &module, 
								   const ADOutlier::AlgoParams &algo_params, const std::string &model_ser); 

    /**
     *@brief A factory function for PSmoduleDataManager instances
     *@param net_nworker The number of worker threads used by the NetInterface
     */
    std::unique_ptr<PSmoduleDataManagerCore> factoryInstantiatePSmoduleDataManager(const std::string &module, int net_nworker);

    /**
     * @brief Return an array of available modules
     */
    std::vector<std::string> factoryListModules();
  }
}
