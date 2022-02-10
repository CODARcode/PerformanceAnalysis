#include<chimbuko_config.h>
#include<chimbuko/param/hbos_param.hpp>
#include "gtest/gtest.h"
#include <sstream>
#include "../unit_test_common.hpp"

using namespace chimbuko;

TEST(TestHistogram, getBin){
  //Create a fake histogram
  std::vector<double> edges = { 0.1, 0.2, 0.3, 0.4, 0.5 };
  std::vector<int> counts = { 1,2,0,4 };

  Histogram h;
  h.set_counts(counts);
  h.set_bin_edges(edges);

  EXPECT_EQ(h.counts().size(), counts.size());
  EXPECT_EQ(h.bin_edges().size(), edges.size());
  
  //Check for data points within the histogram
  EXPECT_EQ( h.getBin(0.15), 0 );
  EXPECT_EQ( h.getBin(0.45), 3 );
  
  //Check for data below histogram and beyond tolerance
  EXPECT_EQ( h.getBin(0.0, 0.05), Histogram::LeftOfHistogram );
  
  //Check for data below histogram but within tolerance
  EXPECT_EQ( h.getBin(0.096, 0.05), 0 );

  //Check for data above histogram and beyond tolerance
  EXPECT_EQ( h.getBin(0.6, 0.05), Histogram::RightOfHistogram );
  
  //Check for data above histogram but within tolerance
  EXPECT_EQ( h.getBin(0.504, 0.05), 3 );  

  //Check lower bin edge is exclusive bound
  EXPECT_EQ( h.getBin(0.1, 0), Histogram::LeftOfHistogram );
  EXPECT_EQ( h.getBin(0.1, 0.01), 0 );
  
  //Check upper bin edge is inclusive
  EXPECT_EQ( h.getBin(0.2), 0 );
  EXPECT_EQ( h.getBin(0.20001), 1 );
  EXPECT_EQ( h.getBin(0.5), 3 );


}

TEST(TestHistogram, createHistogram){
  //Test generation of histogram for which all data points have the same value
  {
    std::vector<double> rtimes = {0.1,0.1,0.1};
    Histogram h;
    h.create_histogram(rtimes);
    
    EXPECT_EQ(h.counts().size(), 1);
    EXPECT_EQ(h.bin_edges().size(), 2);
    EXPECT_EQ(h.counts()[0], 3);
    EXPECT_LT(h.bin_edges()[0], 0.1);
    EXPECT_GT(h.bin_edges()[1], 0.1);
    EXPECT_EQ( h.getBin(0.1), 0 );
  }
  //Test generation of regular histogram with manually chosen bin width
  {
    std::vector<double> rtimes = {0.1,0.1,0.1,0.2,0.4};
    Histogram h;
    h.create_histogram(rtimes, 0.1);

    std::cout << "Regular histogram with manual bin width: " << h << std::endl;

    EXPECT_EQ(h.Nbin(), 4);
    
    //NB: lower edges are exclusive, upper edges inclusive
    EXPECT_EQ( h.getBin(0.1, 0.), 0 );
    EXPECT_EQ( h.getBin(0.2, 0.), 1 );
    EXPECT_EQ( h.getBin(0.3, 0.), 2 );
    EXPECT_EQ( h.getBin(0.4, 0.), 3 );

    EXPECT_EQ( h.getBin(0.0, 0.), Histogram::LeftOfHistogram);
    EXPECT_EQ( h.getBin(0.5, 0.), Histogram::RightOfHistogram);
    
    //Check all data points inside histogram
    int sum = 0;
    for(int c: h.counts()) sum += c;
    
    EXPECT_EQ(sum, rtimes.size());
  }
  //Check that if a data point lies exactly on the upper most bin edge is is still counted
  {
    std::vector<double> rtimes = {0.1,0.1 - 0.001*0.1 + 0.1};
    Histogram h;
    h.create_histogram(rtimes, 0.1);
    
    std::cout << "Another regular histogram with manual bin width: " << h << std::endl;
    
    EXPECT_EQ(h.Nbin(), 1);
    EXPECT_EQ(h.counts()[0], 2);
    EXPECT_EQ(h.getBin(rtimes[0],0), 0);
    EXPECT_EQ(h.getBin(rtimes[1],0), 0);
  }

  //Test generation of regular histogram
  {
    std::vector<double> rtimes = {0.1,0.1,0.1,0.2,0.4};
    Histogram h;
    h.create_histogram(rtimes);
    
    std::cout << "Regular histogram: " << h << std::endl;

    //Check all data points lie within the histogram
    for(double r : rtimes){
      int b = h.getBin(r,0); //0 tolerance
      std::cout << "Got bin " << b << " for runtime " << r << std::endl;

      EXPECT_NE(b, Histogram::LeftOfHistogram);
      EXPECT_NE(b, Histogram::RightOfHistogram);
      EXPECT_LT(h.bin_edges()[b], r); //lower edge exclusive
      EXPECT_GE(h.bin_edges()[b+1], r); //upper edge inclusive
    }      
  }



}

//Find count of values within range start-end (lower bound exclusive, upper inclusive)
int count_in_range(double start, double end, const std::vector<double> &v){
  int out = 0;
  for(auto vv: v){
    if(vv > start && vv <= end) ++out;
  }
  return out;
}


TEST(TestHistogram, mergeTwoHistograms){
  //When the standard deviation is 0 (up to rounding errors) the number of bins diverges and must be carefully treated
  //This should only happen when both histograms have just 1 bin containing data and with the same midpoint
  {
    Histogram g;
    g.set_counts({0,4});
    g.set_bin_edges({0.1,0.2,0.31});

    Histogram l;
    l.set_counts({0,2});
    l.set_bin_edges({0.11,0.2,0.31}); //other bins don't matter
    
    Histogram c = g + l;
    EXPECT_EQ(c.Nbin(),2);
    EXPECT_EQ(c.bin_edges()[0], 0.1);
    EXPECT_EQ(c.bin_edges()[1], 0.2);
    EXPECT_EQ(c.bin_edges()[2], 0.31);

    EXPECT_EQ(c.counts()[0], 0);
    EXPECT_EQ(c.counts()[1], 6);
  }
  //Another test like the above
  {
    Histogram g;
    g.set_counts({0,4});
    g.set_bin_edges({0.1,0.2,0.31+1e-16});

    Histogram l;
    l.set_counts({0,2});
    l.set_bin_edges({0.11,0.2,0.31}); //other bins don't matter
    
    Histogram c = g + l;
    EXPECT_EQ(c.Nbin(),2);
    EXPECT_EQ(c.bin_edges()[0], 0.1);
    EXPECT_EQ(c.bin_edges()[1], 0.2);
    EXPECT_EQ(c.bin_edges()[2], g.bin_edges()[2]);

    EXPECT_EQ(c.counts()[0], 0);
    EXPECT_EQ(c.counts()[1], 6);
  }
  //Do a regular merge for histograms with the same bin width
  //Result will not have the same bin widths because of the rebinning but we can do other checks
  {
    Histogram g;
    g.set_counts({0,1,4,1,0});
    g.set_bin_edges({0.1,0.2,0.3,0.4,0.5,0.6});  

    Histogram l;
    l.set_counts({1,0,6,0,1});
    l.set_bin_edges({0.1,0.2,0.3,0.4,0.5,0.6});

    Histogram c = g + l;

    std::cout << "g : " << g << std::endl;
    std::cout << "l : " << l << std::endl;
    std::cout << "g+l : " << c << std::endl;

    EXPECT_NE(c.Nbin(),0);
    EXPECT_EQ(c.bin_edges().size(), c.Nbin()+1);

    //Compute the midpoint values, which are used as the representative value during the merge
    std::vector<double> midpoints = {0.15,0.25,0.35,0.45,0.55};

    //Check first bin edge is below representative data point (not equal; lower bin edges are exclusive)
    EXPECT_LT(c.bin_edges()[0], midpoints.front());
    //Check last bin edge is equal to or larger than the largest data point
    EXPECT_GE(c.bin_edges()[c.Nbin()], midpoints.back());

    //Check total count is the same
    int gc = 0; 
    for(auto v: g.counts()) gc += v;
    int lc = 0; 
    for(auto v: l.counts()) lc += v;
    int cc = 0; 
    for(auto v: c.counts()) cc += v;

    EXPECT_EQ(gc, g.totalCount());
    EXPECT_EQ(lc, l.totalCount());
    EXPECT_EQ(cc, c.totalCount());       
    
    EXPECT_EQ(cc, gc+lc);

    //Check the bin counts in the output ranges match with the input assuming the input histograms are represented by their midpoint values
    std::vector<double> g_unpacked = g.unpack();
    std::vector<double> l_unpacked = l.unpack();
    EXPECT_EQ(g_unpacked.size(),gc);
    EXPECT_EQ(l_unpacked.size(),lc);
    
    for(int i=0;i<c.Nbin();i++){
      double start = c.bin_edges()[i];
      double end = c.bin_edges()[i+1];

      int nlcl = count_in_range(start,end,l_unpacked);
      int ngbl = count_in_range(start,end,g_unpacked);
      
      EXPECT_EQ(c.counts()[i], nlcl + ngbl);
    }
  }

  //Do a regular merge for histograms with different bin widths
  {
    Histogram g;
    g.set_counts({2,1,1,7,2,1,1});
    double bwg = 0.087;
    std::vector<double> beg(g.Nbin()+1);
    for(int i=0;i<=g.Nbin();i++) beg[i] = 0.09 + i * bwg;
    g.set_bin_edges(beg);

    double smallest = 10000;
    double largest = 0;

    std::vector<double> g_midpoints(g.Nbin());
    for(int i=0;i<g.Nbin();i++){
      g_midpoints[i] = g.bin_edges()[i] + bwg/2.;
      smallest = std::min(g_midpoints[i],smallest);
      largest = std::max(g_midpoints[i],largest);
    }
    

    Histogram l;
    l.set_counts({1,0,6,0,0,6,1});
    double bwl = 0.095;
    std::vector<double> bel(l.Nbin()+1);
    for(int i=0;i<=l.Nbin();i++) bel[i] = 0.093 + i * bwl;
    l.set_bin_edges(bel);

    std::vector<double> l_midpoints(l.Nbin());
    for(int i=0;i<l.Nbin();i++){
      l_midpoints[i] = l.bin_edges()[i] + bwl/2.;
      smallest = std::min(l_midpoints[i],smallest);
      largest = std::max(l_midpoints[i],largest);
    }

    Histogram c = g + l;

    std::cout << "g : " << g << std::endl;
    std::cout << "l : " << l << std::endl;
    std::cout << "g+l : " << c << std::endl;

    EXPECT_NE(c.Nbin(),0);
    EXPECT_EQ(c.bin_edges().size(), c.Nbin()+1);

    //Check first bin edge is below representative data point (not equal; lower bin edges are exclusive)
    EXPECT_LT(c.bin_edges()[0], smallest);
    //Check last bin edge is equal to or larger than the largest data point
    EXPECT_GE(c.bin_edges()[c.Nbin()], largest);

    //Check total count is the same
    int gc = g.totalCount();
    int lc = l.totalCount();
    int cc = c.totalCount();

    EXPECT_EQ(cc, gc+lc);

    //Check the bin counts in the output ranges match with the input assuming the input histograms are represented by their midpoint values
    std::vector<double> g_unpacked = g.unpack();
    std::vector<double> l_unpacked = l.unpack();
    EXPECT_EQ(g_unpacked.size(),gc);
    EXPECT_EQ(l_unpacked.size(),lc);
    
    for(int i=0;i<c.Nbin();i++){
      double start = c.bin_edges()[i];
      double end = c.bin_edges()[i+1];

      int nlcl = count_in_range(start,end,l_unpacked);
      int ngbl = count_in_range(start,end,g_unpacked);
      
      EXPECT_EQ(c.counts()[i], nlcl + ngbl);
    }
  }




}
