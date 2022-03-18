#include<chimbuko_config.h>
#include<chimbuko/param/hbos_param.hpp>
#include "gtest/gtest.h"
#include <sstream>
#include "../unit_test_common.hpp"
#include <boost/math/distributions/normal.hpp>
#include <boost/math/distributions/empirical_cumulative_distribution_function.hpp>

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
  EXPECT_EQ( h.getBin(0.15,0.05), 0 );
  EXPECT_EQ( h.getBin(0.45,0.05), 3 );
  
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
  EXPECT_EQ( h.getBin(0.2, 0.05), 0 );
  EXPECT_EQ( h.getBin(0.20001, 0.05), 1 );
  EXPECT_EQ( h.getBin(0.5, 0.05), 3 );


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
    EXPECT_GE(h.bin_edges()[1], 0.1);
    EXPECT_EQ( h.getBin(0.1,0.05), 0 );
  }
  //Test generation of histogram for which all data points have the same value of 0
  {
    std::vector<double> rtimes = {0.0,0.0,0.0};
    Histogram h;
    h.create_histogram(rtimes);
    
    EXPECT_EQ(h.counts().size(), 1);
    EXPECT_EQ(h.bin_edges().size(), 2);
    EXPECT_EQ(h.counts()[0], 3);
    EXPECT_LT(h.bin_edges()[0], 0.0);
    EXPECT_GE(h.bin_edges()[1], 0.0);
    EXPECT_EQ( h.getBin(0.0,0.05), 0 );
  }

  //Test generation of regular histogram with manually chosen bin width
  {
    std::vector<double> rtimes = {0.1,0.1,0.1,0.2,0.4};
    Histogram h;
    h.create_histogram(rtimes, binWidthFixed(0.1));

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
  //Check that if a data point lies exactly on the upper most bin edge it is still counted
  {
    std::vector<double> rtimes = {0.1,0.1 - 0.01*0.1 + 0.1};
    Histogram h;
    h.create_histogram(rtimes, binWidthFixed(0.1));
    
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
  //Test generation of histogram where lowest value is exactly 0; the lowest bin edge should still be slightly below 0 so as not to exclude it
  {
    std::vector<double> rtimes = {0.0, 0.1,0.1};
    Histogram h;
    h.create_histogram(rtimes);

    EXPECT_LT(h.bin_edges()[0], 0.0);
     int b = h.getBin(0.0, 0);
    EXPECT_EQ(b, 0);
  }

  //Test generation of regular histogram with manually chosen number of bins
  {
    std::vector<double> rtimes = {0.1,0.1,0.1,0.2,0.4};
    Histogram h;
    h.create_histogram(rtimes, binWidthFixedNbin(4));

    std::cout << "Regular histogram with manual nbin: " << h << std::endl;

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

    h.create_histogram(rtimes, binWidthFixedNbin(3));
    EXPECT_EQ(h.Nbin(), 3);

    sum = 0;
    for(int c: h.counts()) sum += c;
    
    EXPECT_EQ(sum, rtimes.size());    

    h.create_histogram(rtimes, binWidthFixedNbin(2));
    EXPECT_EQ(h.Nbin(), 2);

    sum = 0;
    for(int c: h.counts()) sum += c;
    
    EXPECT_EQ(sum, rtimes.size());    

    h.create_histogram(rtimes, binWidthFixedNbin(1));
    EXPECT_EQ(h.Nbin(), 1);

    sum = 0;
    for(int c: h.counts()) sum += c;
    
    EXPECT_EQ(sum, rtimes.size());    
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


TEST(TestHistogram, negation){
  Histogram g;
  g.set_counts({2,40,24,10,3,1,0,1});
  g.set_bin_edges({0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9});

  Histogram n = -g;
  EXPECT_EQ(n.getBin(-0.95, 0.), Histogram::LeftOfHistogram);
  EXPECT_EQ(n.getBin(-0.85, 0.), 0);
  EXPECT_EQ(n.counts()[0], 1);

  EXPECT_EQ(n.getBin(-0.75, 0.), 1);
  EXPECT_EQ(n.counts()[1], 0);

  EXPECT_EQ(n.getBin(-0.65, 0.), 2);
  EXPECT_EQ(n.counts()[2], 1);

  EXPECT_EQ(n.getBin(-0.15, 0.), 7);
  EXPECT_EQ(n.counts()[7], 2);
  
  EXPECT_EQ(n.getBin(-0.05, 0.), Histogram::RightOfHistogram);
}
 

double sum_counts(const std::vector<int> &counts, const int bin){
  double out = 0;
  for(int b=0;b<=bin;b++) out += counts[b];
  return out;
}

TEST(TestHistogram, empiricalCDF){
  Histogram g;
  g.set_counts({2,40,24,10,3,1,0,1});
  g.set_bin_edges({0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9});

  int nbin = g.Nbin();
  EXPECT_EQ(nbin, 8);

  int sum_count = 0;
  for(auto c : g.counts()) sum_count += c;

  std::cout << g << std::endl;
  
  //Test a value below the histogram entirely
  double value = 0.05;
  double cdf = g.empiricalCDF(value);
  std::cout << value << " " << cdf << std::endl;
  EXPECT_EQ(cdf , 0.);

  //Test a value above the histogram entirely
  value = 0.95;
  cdf = g.empiricalCDF(value);
  std::cout << value << " " << cdf << std::endl;
  EXPECT_EQ(cdf , 1.);
  
  //Test a value within the first bin
  value = 0.14;
  cdf = g.empiricalCDF(value);
  double prob = sum_counts(g.counts(),0) / sum_count;
  std::cout << value << " " << cdf << std::endl;
  EXPECT_NEAR(prob, cdf, 1e-3);

  //Test a value within the last bin
  value = 0.851;
  cdf = g.empiricalCDF(value);
  std::cout << value << " " << cdf << std::endl;
  EXPECT_EQ(cdf , 1.);
  
  //Test a value of a middle bin
  value = 0.34;
  cdf = g.empiricalCDF(value);
  prob = sum_counts(g.counts(),2) / sum_count;
  std::cout << value << " " << cdf << std::endl;
  EXPECT_NEAR(prob, cdf, 1e-3);

  //Test with the workspace
  Histogram::empiricalCDFworkspace w;
  value = 0.34;
  cdf = g.empiricalCDF(value, &w);
  prob = sum_counts(g.counts(),2) / sum_count;
  std::cout << value << " " << cdf << std::endl;
  EXPECT_NEAR(prob, cdf, 1e-3);
  
  EXPECT_EQ(w.sum, g.totalCount());
  EXPECT_EQ(w.set, true);

  value = 0.36;
  cdf = g.empiricalCDF(value, &w);
  prob = sum_counts(g.counts(),2) / sum_count;
  std::cout << value << " " << cdf << std::endl;
  EXPECT_NEAR(prob, cdf, 1e-3);  

  //Test the negative empirical CDF 
  Histogram n = -g;
  
  //Inverted:
  //counts:  1,0,1,3,10,24,40,2
  //edges; -0.9,-0.8,-0.7,-0.6,-0.5,-0.4,-0.3,-0.2,-0.1
  std::vector<int> inv_counts = { 1,0,1,3,10,24,40,2 };
  std::vector<double> inv_edges = { -0.9,-0.8,-0.7,-0.6,-0.5,-0.4,-0.3,-0.2,-0.1 };

  
  //Test a value below the histogram entirely
  value = -0.95;
  cdf = n.empiricalCDF(value);
  std::cout << value << " " << cdf << std::endl;
  EXPECT_EQ(cdf , 0.);

  //Test a value above the histogram entirely
  value = 0.0;
  cdf = n.empiricalCDF(value);
  std::cout << value << " " << cdf << std::endl;
  EXPECT_EQ(cdf , 1.);
  
  //Test a value within the first bin
  value = -0.86;
  cdf = n.empiricalCDF(value);
  std::cout << value << " " << cdf << std::endl;
  prob = sum_counts(inv_counts, 0) / sum_count;
  EXPECT_NEAR(prob, cdf, 1e-3);
  
  //Test a value within the last bin
  value = -0.14;
  cdf = n.empiricalCDF(value);
  std::cout << value << " " << cdf << std::endl;
  EXPECT_EQ(cdf , 1.);
  
  //Test a value of a middle bin
  value = -0.26;
  cdf = n.empiricalCDF(value);
  prob = sum_counts(inv_counts, 6) / sum_count;
  std::cout << value << " " << cdf << std::endl;
  EXPECT_NEAR(prob, cdf, 1e-3);
}

TEST(TestHistogram, negatedEmpiricalCDF){
  //For COPOD we need the left-tailed ECDF:   F(x) = fraction of samples of value X_i <= x
  //we also need the right-tailed ECDF:  Fbar(x) = fraction of samples of value X_i >= x

  //Here we check the identity    Fbar(x) = fraction of samples of value -X_i <= -x
  //and that this can be obtained by taking the left-tailed ECDF and performing operator- upon it

  std::vector<double> values = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
  Histogram p;

  //Because the bin is represented by its midpoint value, choose relatively small bin width so easier to test
  p.create_histogram(values, binWidthFixed(0.05));  //first edge just below 1.0

  //std::cout << p << std::endl;
 
  Histogram n = -p;
  
  //std::cout << n << std::endl;

  Histogram::empiricalCDFworkspace w;

  //Check   Fbar(6.0) = n(-6.0) = 1/6
  EXPECT_NEAR(n.empiricalCDF(-6.0), 1./6, 1e-8);
 
  //Check   Fbar(6.1) = n(-6.1) = 0/6
  EXPECT_NEAR(n.empiricalCDF(-6.1), 0./6, 1e-8);

  //Check   Fbar(1.0) = n(-1.0) = 6/6
  EXPECT_NEAR(n.empiricalCDF(-1.0), 6./6, 1e-8);

  //Check   Fbar(1.1) = n(-1.1) = 5/6
  EXPECT_NEAR(n.empiricalCDF(-1.1), 5./6, 1e-8);
}

TEST(TestHistogram, skewness){
  //Create a fake histogram
  std::vector<double> edges = { 0.1, 0.2, 0.3, 0.4, 0.5 };
  std::vector<int> counts = { 8,2,1,4 };
  
  Histogram g;
  g.set_counts(counts);
  g.set_bin_edges(edges);

  //Compute naively by unpacking histogram
  std::vector<double> gu = g.unpack();

  double N = gu.size();
  double mean = 0;
  for(double v : gu) mean += v;
  mean /= N;

  double var = 0.;
  double avg_xmmu_3 = 0.;
  for(double v: gu){
    var += pow(v - mean, 2);
    avg_xmmu_3 += pow(v - mean, 3);
  }
  var /= N;
  avg_xmmu_3 /= (N-1.);
  
  double expect = avg_xmmu_3 / pow(var,3./2);
  double got = g.skewness();
  std::cout << "Skewness got: " << got << " expect " << expect << std::endl;
  EXPECT_NEAR(got, expect, 1e-5);
}

