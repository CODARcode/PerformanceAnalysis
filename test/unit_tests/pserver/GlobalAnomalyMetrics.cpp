#include <random>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include <chimbuko/modules/performance_analysis/pserver/GlobalAnomalyMetrics.hpp>

using namespace chimbuko;

TEST(GlobalAnomalyMetricsTest, TestAggregation){  
  int pid = 0;
  int rid = 1;
  int step1= 2;
  int step2= 3;
  int tid = 3;
  int fid = 4;
  std::string func = "myfunc";

  LocalNet net;
  ADLocalNetClient net_client;
  net_client.connect_ps(rid);

  GlobalAnomalyMetrics glb, glb_net; //test net send route
  net.add_payload(new NetPayloadUpdateAnomalyMetrics(&glb_net));

  //Add first step
  {
    eventID id1(rid,step1,1), id2(rid,step1,2);
    
    ExecData_t 
      e1(id1, pid, rid, tid, fid, func, 100, 200),
      e2(id2, pid, rid, tid, fid, func, 300, 450);
       
    std::list<ExecData_t> calllist;
    calllist.push_back(e1);
    calllist.push_back(e2);
    
    ExecDataMap_t exec_data;
    exec_data[fid].push_back(calllist.begin());
    exec_data[fid].push_back(std::next(calllist.begin()));

    
    ADExecDataInterface iface(&exec_data);
    auto d = iface.getDataSet(0);
    d[0].label = d[1].label = ADDataInterface::EventType::Outlier;
    d[0].score = 3.14;
    d[1].score = 6.28;
    iface.recordDataSetLabels(d,0);
    
    unsigned long first_ts = 100, last_ts = 999;
    
    ADLocalAnomalyMetrics lcl(pid,rid,step1,first_ts,last_ts,iface);
    
    glb.add(lcl);
    lcl.send(net_client); //also send by net
  }  

  auto rec_pit = glb.getRecentMetrics().find(pid);
  ASSERT_NE(rec_pit, glb.getToDateMetrics().end());
  
  auto rec_rit = rec_pit->second.find(rid);
  ASSERT_NE(rec_rit, rec_pit->second.end());

  auto rec_fit = rec_rit->second.find(fid);
  ASSERT_NE(rec_fit, rec_rit->second.end());

  EXPECT_EQ( rec_fit->second.get_score_stats().count(), 2);
  EXPECT_EQ( rec_fit->second.get_first_io_step(), step1 );
  EXPECT_EQ( rec_fit->second.get_last_io_step(), step1 );

  auto run_pit = glb.getToDateMetrics().find(pid);
  ASSERT_NE(run_pit, glb.getToDateMetrics().end());
  
  auto run_rit = run_pit->second.find(rid);
  ASSERT_NE(run_rit, run_pit->second.end());

  auto run_fit = run_rit->second.find(fid);
  ASSERT_NE(run_fit, run_rit->second.end());

  EXPECT_EQ( run_fit->second.get_score_stats().count(), 2);
  EXPECT_EQ( run_fit->second.get_first_io_step(), step1 );
  EXPECT_EQ( run_fit->second.get_last_io_step(), step1 );


  //Test the net-sent version agrees
  EXPECT_EQ(glb, glb_net);

  //Get json and flush
  nlohmann::json json1 = glb.get_json();
  nlohmann::json json1_net = glb_net.get_json();

  EXPECT_EQ(json1.size(), json1_net.size());
  for(int i=0;i<json1.size();i++){
    EXPECT_EQ(json1[i]["all_data"].dump(4), json1_net[i]["all_data"].dump(4));
    EXPECT_EQ(json1[i]["new_data"].dump(4), json1_net[i]["new_data"].dump(4));
  }
    
  //Check flush
  EXPECT_EQ( glb.getRecentMetrics().size(), 0);
  EXPECT_NE( glb.getToDateMetrics().size(), 0);

  //Add second step
  {
    eventID id1(rid,step2,1), id2(rid,step2,2);
    
    ExecData_t 
      e1(id1, pid, rid, tid, fid, func, 500, 200),
      e2(id2, pid, rid, tid, fid, func, 900, 450);
    
    std::list<ExecData_t> calllist;
    calllist.push_back(e1);
    calllist.push_back(e2);

    ExecDataMap_t exec_data;
    exec_data[fid].push_back(calllist.begin());
    exec_data[fid].push_back(std::next(calllist.begin()));

    
    ADExecDataInterface iface(&exec_data);
    auto d = iface.getDataSet(0);
    d[0].label = d[1].label = ADDataInterface::EventType::Outlier;
    d[0].score = 8.88;
    d[1].score = 21.71;
    iface.recordDataSetLabels(d,0);

    unsigned long first_ts = 1000, last_ts = 1999;
    
    ADLocalAnomalyMetrics lcl(pid,rid,step2,first_ts,last_ts,iface);
    
    glb.add(lcl);
    lcl.send(net_client);
  }  

  //Recent statistics should just be the new data
  rec_pit = glb.getRecentMetrics().find(pid);
  ASSERT_NE(rec_pit, glb.getToDateMetrics().end());
  
  rec_rit = rec_pit->second.find(rid);
  ASSERT_NE(rec_rit, rec_pit->second.end());

  rec_fit = rec_rit->second.find(fid);
  ASSERT_NE(rec_fit, rec_rit->second.end());

  EXPECT_EQ( rec_fit->second.get_score_stats().count(), 2);
  EXPECT_EQ( rec_fit->second.get_first_io_step(), step2 );
  EXPECT_EQ( rec_fit->second.get_last_io_step(), step2 );

  //To-date statistics should be all data
  EXPECT_EQ( run_fit->second.get_score_stats().count(), 4);
  EXPECT_EQ( run_fit->second.get_first_io_step(), step1 );
  EXPECT_EQ( run_fit->second.get_last_io_step(), step2 );

  //Test the net-sent version agrees
  EXPECT_EQ(glb, glb_net);
}



TEST(GlobalAnomalyMetricsTest, TestCombination){  
  RunStats stats1,stats2,stats3,stats4;
  for(int i=0;i<100;i++){
    stats1.push(M_PI * (i+1));
    stats2.push(M_PI*M_PI * (i+1));
    stats3.push(M_PI*M_PI*M_PI * (i+1));
    stats4.push(M_PI*M_PI*M_PI*M_PI * (i+1));
  }

  int pid = 111;
  int fid1=99, fid2=33;
  std::string fname1="func1", fname2="func2";
  int count1=122,count2=999;

  //fid1 data 1
  FuncAnomalyMetrics fstate_fid1_1;
  fstate_fid1_1.set_score(stats1);
  fstate_fid1_1.set_severity(stats2);
  fstate_fid1_1.set_count(count1);
  fstate_fid1_1.set_fid(fid1);
  fstate_fid1_1.set_func(fname1);

  //fid1 data 2
  FuncAnomalyMetrics fstate_fid1_2;
  fstate_fid1_2.set_score(stats3);
  fstate_fid1_2.set_severity(stats4);
  fstate_fid1_2.set_count(count2);
  fstate_fid1_2.set_fid(fid1);
  fstate_fid1_2.set_func(fname1);
  
  //fid2 data 1
  FuncAnomalyMetrics fstate_fid2_1;
  fstate_fid2_1.set_score(stats3);
  fstate_fid2_1.set_severity(stats4);
  fstate_fid2_1.set_count(count1);
  fstate_fid2_1.set_fid(fid2);
  fstate_fid2_1.set_func(fname2);
  
  int rid1=22, rid2=77;
  int step1=34, step2=98, step3=101, step4=200;
  int first1 = 1024, first2=2944, first3=8888, first4=10000;
  int last1 = 2888, last2=3004, last3=4000, last4=4232;

  //2 datasets with fid1 and rid1
  ADLocalAnomalyMetrics ldata_fid1_rid1_1;
  ldata_fid1_rid1_1.set_pid(pid);
  ldata_fid1_rid1_1.set_rid(rid1);
  ldata_fid1_rid1_1.set_step(step1);
  ldata_fid1_rid1_1.set_first_event_ts(first1); 
  ldata_fid1_rid1_1.set_last_event_ts(last1);
  ldata_fid1_rid1_1.set_metrics({ {fid1, fstate_fid1_1} });

  ADLocalAnomalyMetrics ldata_fid1_rid1_2;
  ldata_fid1_rid1_2.set_pid(pid);
  ldata_fid1_rid1_2.set_rid(rid1);
  ldata_fid1_rid1_2.set_step(step2);
  ldata_fid1_rid1_2.set_first_event_ts(first2);
  ldata_fid1_rid1_2.set_last_event_ts(last2);
  ldata_fid1_rid1_2.set_metrics({ {fid1, fstate_fid1_2} });

  //1 dataset with fid2 and rid1
  ADLocalAnomalyMetrics ldata_fid2_rid1_1;
  ldata_fid2_rid1_1.set_pid(pid);
  ldata_fid2_rid1_1.set_rid(rid1);
  ldata_fid2_rid1_1.set_step(step3);
  ldata_fid2_rid1_1.set_first_event_ts(first3);
  ldata_fid2_rid1_1.set_last_event_ts(last3);
  ldata_fid2_rid1_1.set_metrics({ {fid2, fstate_fid2_1} });

  //1 dataset with fid1 and rid2
  ADLocalAnomalyMetrics ldata_fid1_rid2_1;
  ldata_fid1_rid2_1.set_pid(pid);
  ldata_fid1_rid2_1.set_rid(rid2);
  ldata_fid1_rid2_1.set_step(step4);
  ldata_fid1_rid2_1.set_first_event_ts(first4);
  ldata_fid1_rid2_1.set_last_event_ts(last4);
  ldata_fid1_rid2_1.set_metrics({ {fid1, fstate_fid1_2} });

  {
    //Data with same fid, different rid
    GlobalAnomalyMetrics g1;
    g1.add(ldata_fid1_rid1_1);

    GlobalAnomalyMetrics g2;
    g2.add(ldata_fid1_rid2_1);

    GlobalAnomalyMetrics gall;
    gall.add(ldata_fid1_rid1_1);
    gall.add(ldata_fid1_rid2_1);

    GlobalAnomalyMetrics gcomb(g1);
    ASSERT_EQ(gcomb, g1);

    gcomb += g2;

    auto pit_all = gall.getRecentMetrics().find(pid);
    ASSERT_NE(pit_all, gall.getRecentMetrics().end());

    auto rit1_all = pit_all->second.find(rid1);
    ASSERT_NE(rit1_all, pit_all->second.end());
    ASSERT_EQ(rit1_all->second.size(),1); //one fid

    auto rit2_all = pit_all->second.find(rid2);
    ASSERT_NE(rit2_all, pit_all->second.end());
    ASSERT_EQ(rit2_all->second.size(),1); //one fid

    auto fit1_all = rit1_all->second.find(fid1);
    ASSERT_NE(fit1_all, rit1_all->second.end());

    auto fit2_all = rit2_all->second.find(fid1);
    ASSERT_NE(fit2_all, rit2_all->second.end());


    auto pit_comb = gcomb.getRecentMetrics().find(pid);
    ASSERT_NE(pit_comb, gcomb.getRecentMetrics().end());

    auto rit1_comb = pit_comb->second.find(rid1);
    ASSERT_NE(rit1_comb, pit_comb->second.end());
    ASSERT_EQ(rit1_comb->second.size(),1); //one fid

    auto rit2_comb = pit_comb->second.find(rid2);
    ASSERT_NE(rit2_comb, pit_comb->second.end());
    ASSERT_EQ(rit2_comb->second.size(),1); //one fid

    auto fit1_comb = rit1_comb->second.find(fid1);
    ASSERT_NE(fit1_comb, rit1_comb->second.end());

    auto fit2_comb = rit2_comb->second.find(fid1);
    ASSERT_NE(fit2_comb, rit2_comb->second.end());

    ASSERT_EQ(fit1_comb->second, fit1_all->second);
    ASSERT_EQ(fit2_comb->second, fit2_all->second);
  }


  {
    //Data with same rid, different fid
    GlobalAnomalyMetrics g1;
    g1.add(ldata_fid1_rid1_1);

    GlobalAnomalyMetrics g2;
    g2.add(ldata_fid2_rid1_1);

    GlobalAnomalyMetrics gall;
    gall.add(ldata_fid1_rid1_1);
    gall.add(ldata_fid2_rid1_1);

    GlobalAnomalyMetrics gcomb(g1);
    ASSERT_EQ(gcomb, g1);

    gcomb += g2;

    auto pit_all = gall.getRecentMetrics().find(pid);
    ASSERT_NE(pit_all, gall.getRecentMetrics().end());

    auto rit1_all = pit_all->second.find(rid1);
    ASSERT_NE(rit1_all, pit_all->second.end());
    ASSERT_EQ(rit1_all->second.size(),2); //two fids

    auto fit1_all = rit1_all->second.find(fid1);
    ASSERT_NE(fit1_all, rit1_all->second.end());

    auto fit2_all = rit1_all->second.find(fid2);
    ASSERT_NE(fit2_all, rit1_all->second.end());


    auto pit_comb = gcomb.getRecentMetrics().find(pid);
    ASSERT_NE(pit_comb, gcomb.getRecentMetrics().end());

    auto rit1_comb = pit_comb->second.find(rid1);
    ASSERT_NE(rit1_comb, pit_comb->second.end());
    ASSERT_EQ(rit1_comb->second.size(),2); //two fids

    auto fit1_comb = rit1_comb->second.find(fid1);
    ASSERT_NE(fit1_comb, rit1_comb->second.end());

    auto fit2_comb = rit1_comb->second.find(fid2);
    ASSERT_NE(fit2_comb, rit1_comb->second.end());

    ASSERT_EQ(fit1_comb->second, fit1_all->second);
    ASSERT_EQ(fit2_comb->second, fit2_all->second);
  }

  {
    //Data with same rid and fid
    GlobalAnomalyMetrics g1;
    g1.add(ldata_fid1_rid1_1);

    GlobalAnomalyMetrics g2;
    g2.add(ldata_fid1_rid1_2);

    GlobalAnomalyMetrics gall;
    gall.add(ldata_fid1_rid1_1);
    gall.add(ldata_fid1_rid1_2);

    GlobalAnomalyMetrics gcomb(g1);
    ASSERT_EQ(gcomb, g1);

    gcomb += g2;

    auto pit_all = gall.getRecentMetrics().find(pid);
    ASSERT_NE(pit_all, gall.getRecentMetrics().end());

    auto rit1_all = pit_all->second.find(rid1);
    ASSERT_NE(rit1_all, pit_all->second.end());
    ASSERT_EQ(rit1_all->second.size(),1); //one fid

    auto fit1_all = rit1_all->second.find(fid1);
    ASSERT_NE(fit1_all, rit1_all->second.end());

    auto pit_comb = gcomb.getRecentMetrics().find(pid);
    ASSERT_NE(pit_comb, gcomb.getRecentMetrics().end());

    auto rit1_comb = pit_comb->second.find(rid1);
    ASSERT_NE(rit1_comb, pit_comb->second.end());
    ASSERT_EQ(rit1_comb->second.size(),1); //one fid

    auto fit1_comb = rit1_comb->second.find(fid1);
    ASSERT_NE(fit1_comb, rit1_comb->second.end());

    ASSERT_EQ(fit1_comb->second, fit1_all->second);


    //Test merge_and_flush
    GlobalAnomalyMetrics g2_cp(g2);
    GlobalAnomalyMetrics gcomb2(g1);
    gcomb2.merge_and_flush(g2_cp);
    EXPECT_EQ(gcomb2, gcomb);
    EXPECT_NE(g2_cp.getToDateMetrics().size(), 0);
    EXPECT_EQ(g2_cp.getRecentMetrics().size(), 0);
  }





}
