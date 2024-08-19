#include<chimbuko/modules/performance_analysis/provdb/ProvDBprune.hpp>
#include<chimbuko/modules/performance_analysis/provdb/ProvDBmoduleSetup.hpp>
#include<chimbuko/core/pserver/PSshardProvenanceDBclient.hpp>
#include<chimbuko/core/pserver/PSglobalProvenanceDBclient.hpp>

#include "gtest/gtest.h"
#include "../../../unit_test_common.hpp"
#include "ProvDBtester.hpp"

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

TEST(TestProvDBprune, works){  
  int nshards = 2;
  ProvDBmoduleSetup setup;
  ProvDBtester pdb(nshards, setup);

  {
    std::ofstream of("/tmp/provider.address.0");
    of << pdb.getAddr();
  }
  std::vector<std::unique_ptr<PSshardProvenanceDBclient> > shard_clients(nshards);
  for(int i=0;i<nshards;i++){
    shard_clients[i].reset(new PSshardProvenanceDBclient(setup.getMainDBcollections()));
    shard_clients[i]->connectShard("/tmp",i,nshards,1);
  }
  
  PSglobalProvenanceDBclient glob_client(setup.getGlobalDBcollections());
  glob_client.connectServer(pdb.getAddr());

  //Put some anomaly data on the shards. Use both shards to check aggregation 
  nlohmann::json anom, norm;

  //event that should be removed
  norm["runtime_exclusive"] = 100;
  norm["fid"] = 1234;
  norm["blah"] = "norm";

  shard_clients[0]->sendData(norm, "anomalies");

  //event that should be kept
  anom["runtime_exclusive"] = 1000;
  anom["fid"] = 1234;
  anom["blah"] = "real_anom1";
  anom["entry"] = 33; //need this info to gather anomaly metrics on kept anomalies
  anom["exit"] = 1033;
  anom["io_step"] = 13;
  anom["outlier_severity"] = 1000;
  anom["rid"] = 88;

  shard_clients[0]->sendData(anom, "anomalies");

  //event that should be removed
  norm["runtime_exclusive"] = 103;
  norm["fid"] = 1234;
  norm["blah"] = "norm2";

  shard_clients[1]->sendData(norm, "anomalies");

  //event that should be kept
  anom["runtime_exclusive"] = 1200;
  anom["fid"] = 1234;
  anom["blah"] = "real_anom2";
  anom["entry"] = 44;
  anom["exit"] = 1244;
  anom["io_step"] = 17;
  anom["outlier_severity"] = 1200;
  anom["rid"] = 88;

  shard_clients[1]->sendData(anom, "anomalies");

  //populate the global database
  nlohmann::json fstats;
  fstats["fid"] = 1234;
  glob_client.sendData(fstats, "func_stats");

  double mean = 100;
  double stddev = 10;
  int count = 1000;
  
  SstdParam param;
  param[1234].set_eta(mean);
  param[1234].set_rho(pow(stddev,2) * (count-1) );
  param[1234].set_count(count);
  ADOutlier::AlgoParams ap; ap.sstd_sigma = 5;

  //Do the business
  ProvDBprune pruner("sstd", ap, param.serialize());
  for(int i=0;i<nshards;i++) pruner.prune(shard_clients[i]->getDatabase());
  pruner.finalize(glob_client.getDatabase());

  //check the shards
  {
    auto sdata = shard_clients[0]->retrieveAllData("anomalies");
    EXPECT_EQ(sdata.size(),1);
    auto s = nlohmann::json::parse(sdata[0]);
    EXPECT_NEAR(s["outlier_score"].template get<double>(), 90, 1e-5);
  }
  {
    auto sdata = shard_clients[1]->retrieveAllData("anomalies");
    EXPECT_EQ(sdata.size(),1);
    auto s = nlohmann::json::parse(sdata[0]);
    EXPECT_NEAR(s["outlier_score"].template get<double>(), 110, 1e-5);
  }
  
  //check the global database
  auto glob_data = glob_client.retrieveAllData("func_stats");
  EXPECT_EQ(glob_data.size(), 1);
  for(auto const &e : glob_data){
    nlohmann::json je = nlohmann::json::parse(e);    
    std::cout << je.dump(4);
    
    EXPECT_EQ(je["anomaly_metrics"]["first_io_step"].template get<int>(), 13);
    EXPECT_EQ(je["anomaly_metrics"]["last_io_step"].template get<int>(), 17);
    EXPECT_EQ(je["anomaly_metrics"]["min_timestamp"].template get<unsigned long>(), 33);
    EXPECT_EQ(je["anomaly_metrics"]["max_timestamp"].template get<unsigned long>(), 1244);
    EXPECT_EQ(je["anomaly_metrics"]["score"]["count"].template get<int>(), 2);
    EXPECT_EQ(je["anomaly_metrics"]["severity"]["count"].template get<int>(), 2);
    EXPECT_EQ(je["anomaly_metrics"]["severity"]["accumulate"].template get<unsigned long>(), 2200);
    EXPECT_EQ(je["anomaly_metrics"]["anomaly_count"]["count"].template get<int>(), 2);  //2 different timesteps
    EXPECT_EQ(je["anomaly_metrics"]["anomaly_count"]["accumulate"].template get<unsigned long>(), 2);
  }
}


