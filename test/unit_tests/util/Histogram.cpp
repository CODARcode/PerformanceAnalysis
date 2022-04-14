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
  std::vector<double> counts = { 1,2,0,4 };

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

  //When the suggested bin size is too small we treat the merge differently
  {
    std::cout << "Test treatment of small bin size suggestions" << std::endl;
    binWidthFixed bw(1e-20);

    Histogram g;
    g.set_counts({1,4});
    g.set_bin_edges({0.1,0.2,0.3});
    g.set_min_max(0.100001,0.3);
    
    Histogram l;
    l.set_counts({3,2});
    l.set_bin_edges({0.0,0.3,0.6}); //other bins don't matter
    l.set_min_max(0.000001,0.6);
    
    Histogram c = Histogram::merge_histograms(g, l, bw);
    
    //It should choose a bin width equal to the smallest of the two bin widths
    EXPECT_NEAR( c.bin_edges()[1] - c.bin_edges()[0], 0.1, 1e-5);

    //Lower edge should be near the shared min
    EXPECT_NEAR( c.bin_edges()[0], 0.001,  1e-3);

    //Upper edge should be near the shared max
    EXPECT_NEAR( c.bin_edges()[c.Nbin()], 0.6,  1e-3);

    //Number of bins should not be large
    EXPECT_LT( c.Nbin(), 100 );    
  }
  //Do a regular merge for histograms with the same bin width
  //Result will not have the same bin widths because of the rebinning but we can do other checks
  {
    std::cout << "Regular merge for histograms with same bin width" << std::endl;
    Histogram g;
    g.set_counts({0,1,4,1,0});
    g.set_bin_edges({0.1,0.2,0.3,0.4,0.5,0.6});  
    g.set_min_max(0.1+1e-8,0.6);

    Histogram l;
    l.set_counts({1,0,6,0,1});
    l.set_bin_edges({0.1,0.2,0.3,0.4,0.5,0.6});
    l.set_min_max(0.1+1e-8,0.6);


    Histogram c = Histogram::merge_histograms(g, l);

    std::cout << "g : " << g << std::endl;
    std::cout << "l : " << l << std::endl;
    std::cout << "g+l : " << c << std::endl;

    EXPECT_NE(c.Nbin(),0);
    EXPECT_EQ(c.bin_edges().size(), c.Nbin()+1);

    //Check first bin edge is below the minimum data value (not equal; lower bin edges are exclusive)
    EXPECT_LT(c.bin_edges()[0], 0.101);
    //Check last bin edge is equal to or larger than the largest data point
    EXPECT_GE(c.bin_edges()[c.Nbin()], 0.6);

    //Check total count is the same
    double gc = 0; 
    for(auto v: g.counts()) gc += v;
    double lc = 0; 
    for(auto v: l.counts()) lc += v;
    double cc = 0; 
    for(auto v: c.counts()) cc += v;

    EXPECT_EQ(gc, g.totalCount());
    EXPECT_EQ(lc, l.totalCount());
    EXPECT_EQ(cc, c.totalCount());       
    
    EXPECT_NEAR(cc, gc+lc, 1e-12);
  }

  //Do a regular merge for histograms with different bin widths
  {
    std::cout << "Regular merge for histograms with different bin width" << std::endl;
    Histogram g;
    g.set_counts({2,1,1,7,2,1,1});
    double bwg = 0.087;
    std::vector<double> beg(g.Nbin()+1);
    for(int i=0;i<=g.Nbin();i++) beg[i] = 0.09 + i * bwg;
    g.set_bin_edges(beg);
    g.set_min_max(beg.front()+1e-8, beg.back());

    Histogram l;
    l.set_counts({1,0,6,0,0,6,1});
    double bwl = 0.095;
    std::vector<double> bel(l.Nbin()+1);
    for(int i=0;i<=l.Nbin();i++) bel[i] = 0.093 + i * bwl;
    l.set_bin_edges(bel);
    l.set_min_max(bel.front()+1e-8, bel.back());

    double smallest = std::min(beg.front()+1e-8,bel.front()+1e-8);
    double largest = std::max(beg.back(),bel.back());

    Histogram c = Histogram::merge_histograms(g, l);

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
    double gc = g.totalCount();
    double lc = l.totalCount();
    double cc = c.totalCount();

    EXPECT_NEAR(cc, gc+lc, 1e-12);
  }


}


TEST(TestHistogram, negation){
  Histogram g;
  g.set_counts({2,40,24,10,3,1,0,1});
  g.set_bin_edges({0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9});
  g.set_min_max(0.11,0.9);

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

  EXPECT_EQ(n.getMax(), -0.11);
  EXPECT_EQ(n.getMin(), -0.9);
}
 

double sum_counts(const std::vector<double> &counts, const int bin){
  double out = 0;
  for(int b=0;b<=bin;b++) out += counts[b];
  return out;
}

TEST(TestHistogram, empiricalCDF){
  Histogram g;
  g.set_counts({2,40,24,10,3,1,0,1});
  g.set_bin_edges({0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9});
  g.set_min_max(0.1+1e-8,0.9);

  int nbin = g.Nbin();
  EXPECT_EQ(nbin, 8);

  double sum_count = 0;
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
  double prob = (value-0.1)/(0.2-0.1)*2./sum_count;
  std::cout << value << " " << cdf << std::endl;
  EXPECT_NEAR(prob, cdf, 1e-3);

  //Test a value within the last bin
  value = 0.851;
  cdf = g.empiricalCDF(value);
  prob = ( sum_counts(g.counts(), nbin-2)   +  (value - 0.8)/0.1*1. )/sum_count;
  std::cout << value << " " << cdf << std::endl;
  EXPECT_NEAR(cdf , prob, 1e-3);
  
  //Test a value of a middle bin
  value = 0.34;
  cdf = g.empiricalCDF(value);
  prob = (sum_counts(g.counts(),1) + (value-0.3)/0.1*24. )/ sum_count;
  std::cout << value << " " << cdf << std::endl;
  EXPECT_NEAR(prob, cdf, 1e-3);

  //Test with the workspace
  Histogram::empiricalCDFworkspace w;
  value = 0.34;
  cdf = g.empiricalCDF(value, &w);
  std::cout << value << " " << cdf << std::endl;
  EXPECT_NEAR(prob, cdf, 1e-3);
  
  EXPECT_EQ(w.sum, g.totalCount());
  EXPECT_EQ(w.set, true);

  value = 0.36;
  cdf = g.empiricalCDF(value, &w);
  prob = (sum_counts(g.counts(),1) + (value-0.3)/0.1*24.)/ sum_count;
  std::cout << value << " " << cdf << std::endl;
  EXPECT_NEAR(prob, cdf, 1e-3);  

  //Test the negative empirical CDF 
  Histogram n = -g;
  
  //Inverted:
  //counts:  1,0,1,3,10,24,40,2
  //edges; -0.9,-0.8,-0.7,-0.6,-0.5,-0.4,-0.3,-0.2,-0.1
  std::vector<double> inv_counts = { 1,0,1,3,10,24,40,2 };
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
  prob = (value - inv_edges[0])/0.1 * inv_counts[0] / sum_count;
  EXPECT_NEAR(prob, cdf, 1e-3);
  
  //Test a value within the last bin
  value = -0.14;
  cdf = n.empiricalCDF(value);
  prob = (sum_counts(inv_counts,6) + (value - (-0.2))/0.1 * inv_counts.back())/sum_count;
  std::cout << value << " " << cdf << std::endl;
  EXPECT_EQ(cdf , prob);
  
  //Test a value of a middle bin
  value = -0.26;
  cdf = n.empiricalCDF(value);
  prob = ( sum_counts(inv_counts, 5) + (value - (-0.3))/0.1 * 40. )/ sum_count;
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

  //std::cout << "p:" << std::endl << p << std::endl;
 
  Histogram n = -p;
  
  //std::cout << "n:" << std::endl << n << std::endl;

  Histogram::empiricalCDFworkspace w;

  //Check   Fbar(5.94) = n(-5.94) = 1./6  (should include entire last bin)
  EXPECT_NEAR(n.empiricalCDF(-5.94), 1./6, 1e-8);

  //Check   Fbar(6.0) = n(-6.0) = 0/6
  EXPECT_NEAR(n.empiricalCDF(-6.0), 0., 1e-8);
 
  //Check   Fbar(6.1) = n(-6.1) = 0/6
  EXPECT_NEAR(n.empiricalCDF(-6.1), 0./6, 1e-8);

  //Check   Fbar(0.9999) = n(-0.999) = 6/6  (should include entire first bin)
  EXPECT_NEAR(n.empiricalCDF(-0.999), 6./6, 1e-8);

  //Check   Fbar(1.1) = n(-1.1) = 5/6
  EXPECT_NEAR(n.empiricalCDF(-1.1), 5./6, 1e-8);
}

TEST(TestHistogram, skewness){
  //Create a fake histogram
  std::vector<double> edges = { 0.1, 0.2, 0.3, 0.4, 0.5 };
  std::vector<double> counts = { 8,2,1,4 };
  
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

TEST(TestHistogram, uniformCountInRange){
  //Check for a normal histogram
  {
    Histogram h;
    h.set_bin_edges({0,1,2,3});
    h.set_counts({1,2,3});
    h.set_min_max(1e-12, 3);
    
    //total sum
    EXPECT_NEAR(h.uniformCountInRange(0,3),6.,1e-5);
    
    //last bin
    EXPECT_NEAR(h.uniformCountInRange(2,3),3.,1e-5);

    //partial bin
    EXPECT_NEAR(h.uniformCountInRange(2.5,3),3./2,1e-5);
    
    //multiple partial bins
    EXPECT_NEAR(h.uniformCountInRange(0.5,2.5), 0.5+2+1.5,1e-5);
  }
  //Check special treatment for a histogram with min=max
  {
    Histogram h;
    h.set_bin_edges({0,1});
    h.set_counts({3});
    h.set_min_max(0.5, 0.5);
    
    EXPECT_NEAR(h.uniformCountInRange(0.499,0.5),3.,1e-5);
    EXPECT_NEAR(h.uniformCountInRange(0.5,0.501),0.,1e-5);
  }

}


struct HistogramTest: public Histogram{
  using Histogram::merge_histograms_uniform;


};


TEST(TestHistogram, mergeUniform){
  //Test merge when bin edges align
  {
    HistogramTest h;
    h.set_bin_edges({0,1,2,3});
    h.set_counts({0,0,0});
    
    HistogramTest l;
    l.set_bin_edges({0,1,2,3});
    l.set_counts({0,3,0});

    HistogramTest g;
    g.set_bin_edges({0,1,2,3});
    g.set_counts({2,0,1});

    HistogramTest::merge_histograms_uniform(h,l,g);

    EXPECT_EQ(h.binCount(0),2);
    EXPECT_EQ(h.binCount(1),3);
    EXPECT_EQ(h.binCount(2),1);
  }

  //Test merge when bin edges don't align
  {
    HistogramTest h;
    h.set_bin_edges({-1,0,1,2,3,4,5});
    h.set_counts({0,0,0,0,0,0});
    
    HistogramTest l;
    l.set_bin_edges({-1,1,3,5});
    l.set_counts({4,0,0});

    EXPECT_NEAR(l.uniformCountInRange(0,1), 2.0, 0.001);

    HistogramTest g;
    g.set_bin_edges({-1,1,2,3});
    g.set_counts({6,0,0});

    EXPECT_NEAR(g.uniformCountInRange(0,1), 3.0, 0.001);


    HistogramTest::merge_histograms_uniform(h,l,g);

    EXPECT_EQ(h.binCount(0),5); //expect 1/2 of the first bin from each
    EXPECT_EQ(h.binCount(1),5); //same for the second bin

    for(int i=2;i<6;i++) EXPECT_EQ(h.binCount(i),0);
  }


  //Test with fractional counts
  {
    HistogramTest h;
    h.set_bin_edges({-1,-0.5,0,0.5,1,1.5,2.0,2.5,3.0});
    h.set_counts({0,0,0,0,0,0,0,0});
    
    HistogramTest l;
    l.set_bin_edges({-1,1,3});
    l.set_counts({3,0});

    EXPECT_NEAR(l.uniformCountInRange(-1,-0.5), 0.75, 0.001); //first 4 bins will get 0.75 from l
    EXPECT_NEAR(l.uniformCountInRange(1,1.5), 0.0, 0.001); //last 4 bins will get 0 from l

    HistogramTest g;
    g.set_bin_edges({-1,1,3});
    g.set_counts({0,2});

    EXPECT_NEAR(g.uniformCountInRange(-1,-0.5), 0.0, 0.001); //first 4 bins will get 0 from g
    EXPECT_NEAR(g.uniformCountInRange(1,1.5), 0.5, 0.001); //last 4 bins will get 0.5 from g rounded up to 1

    HistogramTest::merge_histograms_uniform(h,l,g);

    //Total count will be 8 but it should be 5, and as the bins all have value 1, it will subtract 1 from the first 3 bins (note this is something of an edge case)
    for(int i=0;i<4;i++) EXPECT_NEAR(h.binCount(i),0.75,1e-12);
    for(int i=4;i<8;i++) EXPECT_NEAR(h.binCount(i),0.5,1e-12);
  }
}


//Test the correction used for COPOD where we shift the bin edges such that the CDF of the min value is 1/N rather than 0
TEST(TestHistogram, empiricalCDFshift){
  Histogram g;
  g.set_counts({2,6,5});
  g.set_bin_edges({0.1,0.2,0.3,0.4});
  g.set_min_max(0.1+1e-8,0.4);

  //Check initial CDF is ~0
  double min = g.getMin();
  EXPECT_NEAR(g.empiricalCDF(min), 0., 1e-4);

  double n = g.totalCount();
  
  //Require  (min - edge)/bin_width * bin_count[0] / n = 1./n
  //edge = min - bin_width/bin_count[0]
  double shift = -0.1/2;

  g.shiftBinEdges(shift);
  
  double cdf = g.empiricalCDF(min);
  double desire = 1./n;
  std::cout << min << " " << cdf << " desire " << desire << std::endl;
  EXPECT_NEAR(cdf , desire, 1e-6);
}
