#include "chimbuko/param/hbos_param.hpp"
#include "gtest/gtest.h"
#include <sstream>
#include <limits>
#include <random>

using namespace chimbuko;

TEST(TestHbosParam, generateLocalHistograms){
  std::default_random_engine gen(1234);
  std::normal_distribution<double> dist(50.,10.);
  int N = 50;
  int fid = 123;

  double min = std::numeric_limits<double>::max(), max=std::numeric_limits<double>::lowest();
  std::vector<double> runtimes(N);
  for(int i=0;i<N;i++){
    double v = dist(gen);
    runtimes[i] = v;
    min = std::min(v,min);
    max = std::max(v,max);      
  }
  
  //For first test, do not bound the number of bins
  {    
    binWidthScott bw;
    Histogram expect(runtimes, bw);
    
    HbosParam p;
    p.generate_histogram(fid, runtimes);

    auto const &stats = p.get_hbosstats();
    auto it = stats.find(fid);
    ASSERT_NE(it, stats.end());
    
    EXPECT_EQ(it->second, expect);
  }

  //Bound the number of bins
  {    
    int maxbins = 2;
    //Check that the unbounded Scott rule would want more bins than maxbins

    double bwscott = Histogram::scottBinWidth(runtimes);
    int nbinscott = int(ceil( (max-min)/bwscott + Histogram::getLowerBoundShiftMul()));
    EXPECT_GT(nbinscott, maxbins);

    binWidthScottMaxNbin bw(maxbins);
    Histogram expect(runtimes, bw);
    
    EXPECT_EQ(expect.Nbin(),maxbins);

    HbosParam p;
    p.setMaxBins(maxbins);
    p.generate_histogram(fid, runtimes);

    auto const &stats = p.get_hbosstats();
    auto it = stats.find(fid);
    ASSERT_NE(it, stats.end());
    
    EXPECT_EQ(it->second, expect);
  }

}




TEST(TestHbosParam, syncLocalGlobal){
  std::default_random_engine gen(1234);
  std::normal_distribution<double> dist(50.,10.);
  int N = 50;
  int fid = 123;
  int maxbins = 2;  

  double min = std::numeric_limits<double>::max(), max=std::numeric_limits<double>::lowest();
  std::vector<double> runtimes(N);
  for(int i=0;i<N;i++){
    double v = dist(gen);
    runtimes[i] = v;
    min = std::min(v,min);
    max = std::max(v,max);
  }
  double min2 = std::numeric_limits<double>::max(), max2=std::numeric_limits<double>::lowest();
  std::vector<double> runtimes2(N);
  for(int i=0;i<N;i++){
    double v = dist(gen);
    runtimes2[i] = v;
    min2 = std::min(v,min);
    max2 = std::max(v,max);
  }

  //Check that the unbounded Scott rule would want more bins than maxbins  
  double bwscott = Histogram::scottBinWidth(runtimes);
  int nbinscott = int(ceil( (max-min)/bwscott + Histogram::getLowerBoundShiftMul()));
  EXPECT_GT(nbinscott, maxbins);

  bwscott = Histogram::scottBinWidth(runtimes2);
  nbinscott = int(ceil( (max2-min2)/bwscott + Histogram::getLowerBoundShiftMul()));
  EXPECT_GT(nbinscott, maxbins);

  
  HbosParam l;
  l.setMaxBins(maxbins);
  l.generate_histogram(fid, runtimes);

  binWidthScottMaxNbin bw(maxbins);
  Histogram expect(runtimes, bw);
  EXPECT_EQ(expect.Nbin(), maxbins);

  ASSERT_TRUE(l.find(fid));
  EXPECT_EQ(l[fid], expect);
  
  //Don't set the maxbins on the global histogram so we can check they are propagated from the local data
  HbosParam g;
  EXPECT_NE(g.getMaxBins(), maxbins);

  g.update_and_return(l); //l and g are the same; the return should not affect l here as the initial global histogram is empty
  EXPECT_EQ(g.getMaxBins(), maxbins);
  
  EXPECT_EQ(l[fid], expect);
  ASSERT_TRUE(g.find(fid));
  EXPECT_EQ(g[fid], expect);

  //Make local model from second data set
  Histogram h2(runtimes2, bw);
  expect = Histogram::merge_histograms(expect, h2, bw);
  EXPECT_EQ(expect.Nbin(), maxbins);  

  l.generate_histogram(fid, runtimes2);
  ASSERT_TRUE(l.find(fid));
  EXPECT_EQ(l[fid], h2);

  g.update_and_return(l);
  EXPECT_EQ(l[fid], expect);
  ASSERT_TRUE(g.find(fid));
  EXPECT_EQ(g[fid], expect);
}  
