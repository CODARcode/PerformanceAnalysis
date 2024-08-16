#include<chimbuko/modules/performance_analysis/provdb/ProvDBpruneInterface.hpp>
#include<chimbuko/modules/performance_analysis/provdb/ProvDBmoduleSetup.hpp>
#include<chimbuko/core/ad/ADProvenanceDBclient.hpp>

#include "gtest/gtest.h"
#include "../../../unit_test_common.hpp"
#include "ProvDBtester.hpp"

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

TEST(TestProvDBpruneOutlierInterface, basic){  
  int nshards = 1;
  ProvDBmoduleSetup setup;
  ProvDBtester pdb(nshards, setup);

  ADProvenanceDBclient main_client(setup.getMainDBcollections(), 0);
  main_client.connectSingleServer(pdb.getAddr(),nshards);

  nlohmann::json anom;
  anom["runtime_exclusive"] = 100;
  anom["fid"] = 1234;
  anom["blah"] = "norm";

  main_client.sendData(anom, "anomalies");

  anom["runtime_exclusive"] = 1000;
  anom["fid"] = 1234;
  anom["blah"] = "real_anom1";

  main_client.sendData(anom, "anomalies");

  anom["runtime_exclusive"] = 1200;
  anom["fid"] = 1234;
  anom["blah"] = "real_anom2";

  main_client.sendData(anom, "anomalies");

  double mean = 100;
  double stddev = 10;
  int count = 1000;
  
  SstdParam param;
  param[1234].set_eta(mean);
  param[1234].set_rho(pow(stddev,2) * (count-1) );
  param[1234].set_count(count);
  
  EXPECT_EQ( main_client.retrieveAllData("anomalies").size(), 3 );

  ADOutlier::AlgoParams ap; ap.sstd_sigma = 5;
  {
    std::unique_ptr<ADOutlier> ad(ADOutlier::set_algorithm(0,"sstd",ap));
    ad->setGlobalParameters(param.serialize()); //input model
    ad->setGlobalModelSyncFrequency(0); //fix model
    ProvDBpruneInterface pi(*ad, pdb.getShard(0));
    ad->run(pi);
  }

  auto all_data = main_client.retrieveAllData("anomalies");
  EXPECT_EQ(all_data.size(), 2);
  for(auto const &e : all_data){
    nlohmann::json je = nlohmann::json::parse(e);
    EXPECT_GE(je["runtime_exclusive"].template get<double>(), 1000);
    std::cout << je.dump(4) << std::endl;
  }

}

