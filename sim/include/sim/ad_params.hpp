#include<chimbuko/param/sstd_param.hpp>
#include<chimbuko/ad/ADOutlier.hpp>
#include "id_map.hpp"

namespace chimbuko_sim{
  using namespace chimbuko;

  struct ADalgParams{
    std::string algorithm;
    double hbos_thres;
    bool glob_thres;
    double sstd_sigma;
    ADOutlier::OutlierStatistic stat;
    
    ADalgParams(const std::string &algorithm, double hbos_thres, bool glob_thres, double sstd_sigma, ADOutlier::OutlierStatistic stat): algorithm(algorithm), hbos_thres(hbos_thres), glob_thres(glob_thres), sstd_sigma(sstd_sigma), stat(stat){}

    /**
     * @brief Default parameters
     */
    ADalgParams(): algorithm("none"), hbos_thres(0.99), glob_thres(true), sstd_sigma(12.0), stat(ADOutlier::ExclusiveRuntime){}
  };
  
  /**
   * @brief Set the parameters of the ad algorithm
   *
   * If algorithm=="none" (defulat) the user is expected to tag events manually (the params put in the provDB will use the SSTD algorithm format), otherwise it will use one of Chimbuko's algorithms
   */ 
  inline ADalgParams & adAlgorithmParams(){ static ADalgParams alg; return alg; }


  /**
   * @brief If an AD algorithm is not used (ad_algorithm() == "none" [default]) this is the object containing the params that are placed in the provDB
   */
  inline SstdParam & globalParams(){ static SstdParam p; return p; }
  
  /**
   * @brief If an AD algorithm is not used (ad_algorithm() == "none" [default]) all functions should be 'registered' using this command
   *
   * This function sets up both the function idx map and enters the provided statistics in the globalParams() instance
   */
  void registerFunc(const std::string &func_name, unsigned long mean_runtime, unsigned long std_dev_runtime, int seen_count);
};
