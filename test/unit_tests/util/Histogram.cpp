#include<chimbuko_config.h>
#include<chimbuko/core/param/hbos_param.hpp>
#include<chimbuko/util/serialize.hpp>
#include "gtest/gtest.h"
#include <sstream>
#include "../unit_test_common.hpp"
#include <boost/math/distributions/normal.hpp>
#include <boost/math/distributions/empirical_cumulative_distribution_function.hpp>

using namespace chimbuko;


TEST(TestHistogram, setHistogram){
  {
    std::cout << "setHistogram checking valid histogram" << std::endl;
    Histogram h;
    h.set_histogram({1,2,0,4}, 0.1+1e-8,0.5,  0.1,0.1);
    EXPECT_EQ(h.Nbin(),4);
    EXPECT_EQ(h.binWidth(),0.1);
    EXPECT_EQ(h.getMin(),0.1+1e-8);
    EXPECT_EQ(h.getMax(),0.5);
    EXPECT_EQ(h.binEdgeLower(0),0.1);
    EXPECT_NEAR(h.binEdgeUpper(0),0.2,1e-8);
    EXPECT_EQ(h.binEdgeUpper(3),0.5);
    EXPECT_EQ(h.binCount(0),1);
    EXPECT_EQ(h.binCount(1),2);
    EXPECT_EQ(h.binCount(2),0);
    EXPECT_EQ(h.binCount(3),4);
  }
  {
    std::cout << "setHistogram check it throws an error when the upper edge doesn't match the max" << std::endl;
    Histogram h;
    EXPECT_ANY_THROW( h.set_histogram({1,2,0,4}, 0.1+1e-8,0.51,  0.1,0.1) );
  }
  {
    std::cout << "setHistogram check it throws an error if max < min" << std::endl;
    Histogram h;
    EXPECT_ANY_THROW( h.set_histogram({1,2,0,4}, 2,1,  0.1,0.1) );
  }
  {
    std::cout << "setHistogram check it throws an error if min is not inside the first bin" << std::endl;
    Histogram h;
    EXPECT_ANY_THROW( h.set_histogram({1,2,0,4}, 0.21,0.5,  0.1,0.1) );
    EXPECT_ANY_THROW( h.set_histogram({1,2,0,4}, 0.09,0.5,  0.1,0.1) );
  }
}

TEST(TestHistogram, getBin){
  //Create a fake histogram
  std::vector<Histogram::CountType> counts = { 1,2,0,4 };

  Histogram h;
  h.set_histogram(counts, 0.11, 0.5, 0.1, 0.1);

  EXPECT_EQ(h.counts().size(), counts.size());
  
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
  EXPECT_EQ( h.getBin(0.19999, 0.05), 0 );
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
    EXPECT_EQ(h.counts()[0], 3);
    EXPECT_LT(h.binEdgeLower(0), 0.1);
    EXPECT_GE(h.binEdgeUpper(0), 0.1);
    EXPECT_EQ( h.getBin(0.1,0.05), 0 );
  }
  //Test generation of histogram for which all data points have the same value of 0
  {
    std::vector<double> rtimes = {0.0,0.0,0.0};
    Histogram h;
    h.create_histogram(rtimes);
    
    EXPECT_EQ(h.counts().size(), 1);
    EXPECT_EQ(h.counts()[0], 3);
    EXPECT_LT(h.binEdgeLower(0), 0.0);
    EXPECT_GE(h.binEdgeUpper(1), 0.0);
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
      EXPECT_LT(h.binEdgeLower(b), r); //lower edge exclusive
      EXPECT_GE(h.binEdgeUpper(b), r); //upper edge inclusive
    }      
  }
  //Test generation of histogram where lowest value is exactly 0; the lowest bin edge should still be slightly below 0 so as not to exclude it
  {
    std::vector<double> rtimes = {0.0, 0.1,0.1};
    Histogram h;
    h.create_histogram(rtimes);

    EXPECT_LT(h.binEdgeLower(0), 0.0);
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
    Histogram::CountType sum = 0;
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
    g.set_histogram({1,4}, 0.100001,0.3, 0.1, 0.1);
    
    Histogram l;
    l.set_histogram({3,2}, 0.000001,0.6, 0.0, 0.3);
    
    Histogram c = Histogram::merge_histograms(g, l, bw);
    
    EXPECT_NEAR( c.binWidth(), 0.1, 1e-4);

    //Lower edge should be near the shared min
    EXPECT_NEAR( c.binEdgeLower(0), 0.001,  1e-3);

    //Upper edge should be equal to the shared max
    EXPECT_EQ( c.binEdgeUpper(c.Nbin()-1), 0.6);

    //Number of bins should not be large
    EXPECT_LT( c.Nbin(), 100 );    
  }
  //Do a regular merge for histograms with the same bin width
  //Result will not have the same bin widths because of the rebinning but we can do other checks
  {
    std::cout << "Regular merge for histograms with same bin width" << std::endl;
    Histogram g;
    g.set_histogram({0,1,4,1,0}, 0.1+1e-8,0.6, 0.1, 0.1);

    Histogram l;
    l.set_histogram({1,0,6,0,1}, 0.1+1e-8,0.6, 0.1, 0.1);

    Histogram c = Histogram::merge_histograms(g, l);

    std::cout << "g : " << g << std::endl;
    std::cout << "l : " << l << std::endl;
    std::cout << "g+l : " << c << std::endl;

    EXPECT_NE(c.Nbin(),0);

    //Check first bin edge is below the minimum data value (not equal; lower bin edges are exclusive)
    EXPECT_LT(c.binEdgeLower(0), 0.101);
    //Check last bin edge is equal to shared max
    EXPECT_EQ(c.binEdgeUpper(c.Nbin()-1), 0.6);

    //Check total count is the same
    Histogram::CountType gc = 0; 
    for(auto v: g.counts()) gc += v;
    Histogram::CountType lc = 0; 
    for(auto v: l.counts()) lc += v;
    Histogram::CountType cc = 0; 
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
    double bwg = 0.087;
    std::vector<double> beg(8);
    for(int i=0;i<8;i++) beg[i] = 0.09 + i * bwg;
    g.set_histogram({2,1,1,7,2,1,1}, beg.front()+1e-8, beg.back(), beg.front(), bwg);

    Histogram l;
    double bwl = 0.095;
    std::vector<double> bel(8);
    for(int i=0;i<8;i++) bel[i] = 0.093 + i * bwl;
    l.set_histogram({1,0,6,0,0,6,1},bel.front()+1e-8, bel.back(), bel.front(), bwl);   

    double smallest = std::min(beg.front()+1e-8,bel.front()+1e-8);
    double largest = std::max(beg.back(),bel.back());

    Histogram c = Histogram::merge_histograms(g, l);

    std::cout << "g : " << g << std::endl;
    std::cout << "l : " << l << std::endl;
    std::cout << "g+l : " << c << std::endl;

    EXPECT_NE(c.Nbin(),0);

    //Check first bin edge is below representative data point (not equal; lower bin edges are exclusive)
    EXPECT_LT(c.binEdgeLower(0), smallest);
    //Check last bin edge is equal to the largest data point
    EXPECT_EQ(c.binEdgeUpper(c.Nbin()-1), largest);

    //Check total count is the same
    Histogram::CountType gc = g.totalCount();
    Histogram::CountType lc = l.totalCount();
    Histogram::CountType cc = c.totalCount();

    EXPECT_EQ(cc, gc+lc);
  }


}


TEST(TestHistogram, negation){
  Histogram g;
  g.set_histogram({2,40,24,10,3,1,0,1}, 0.11,0.9, 0.1,0.1);

  Histogram n = -g;

  std::cout << "Histogram : " << g << std::endl;
  std::cout << "Negated histogram : " << n << std::endl;

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
 

Histogram::CountType sum_counts(const std::vector<Histogram::CountType> &counts, const int bin){
  Histogram::CountType out = 0;
  for(int b=0;b<=bin;b++) out += counts[b];
  return out;
}

TEST(TestHistogram, empiricalCDF){
  Histogram g;
  g.set_histogram({2,40,24,10,3,1,0,1}, 0.1+1e-8,0.9, 0.1, 0.1);

  int nbin = g.Nbin();
  EXPECT_EQ(nbin, 8);

  Histogram::CountType sum_count = 0;
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
  std::vector<Histogram::CountType> inv_counts = { 1,0,1,3,10,24,40,2 };
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
  EXPECT_NEAR(cdf , prob, 1e-3);
  
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
  EXPECT_NEAR(n.empiricalCDF(-5.94), 1./6, 1e-6);

  //Check   Fbar(6.0) = n(-6.0) = 0/6
  EXPECT_NEAR(n.empiricalCDF(-6.0), 0., 1e-6);
 
  //Check   Fbar(6.1) = n(-6.1) = 0/6
  EXPECT_NEAR(n.empiricalCDF(-6.1), 0./6, 1e-6);

  //Check   Fbar(0.9999) = n(-0.999) = 6/6  (should include entire first bin)
  EXPECT_NEAR(n.empiricalCDF(-0.999), 6./6, 1e-6);

  //Check   Fbar(1.1) = n(-1.1) = 5/6
  EXPECT_NEAR(n.empiricalCDF(-1.1), 5./6, 1e-6);
}

TEST(TestHistogram, skewness){
  //Create a fake histogram
  std::vector<Histogram::CountType> counts = { 8,2,1,4 };
  
  Histogram g;
  g.set_histogram(counts, 0.11,0.5, 0.1,0.1);

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
    h.set_histogram({1,2,3}, 1e-12,3  , 0,1);
    
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
    h.set_histogram({3}, 0.5,0.5, 0.5-1e-8,1e-8);
    
    EXPECT_NEAR(h.uniformCountInRange(0.499,0.5),3.,1e-5);
    EXPECT_NEAR(h.uniformCountInRange(0.5,0.501),0.,1e-5);
  }

}

TEST(TestHistogramVBW, TestBasics){
  Histogram h;
  h.set_histogram({1,2,3}, 1e-12, 3,  0,1);
  
  HistogramVBW hv;
  EXPECT_EQ(hv.Nbin(),0);
  
  hv.import(h);
  
  typedef HistogramVBW::Bin Bin;

  Bin const* bins[3];

  ASSERT_EQ(hv.Nbin(),3);
  Bin const* b = hv.getBinByIndex(0);
  ASSERT_NE(b,nullptr);
  EXPECT_EQ(b->l, 0);
  EXPECT_EQ(b->u, 1);
  EXPECT_EQ(b->c, 1);
  EXPECT_EQ(b->left, nullptr);

  bins[0] = b;
  Bin const* bp = b;
  Bin const* bn = b->right;

  b = hv.getBinByIndex(1);
  ASSERT_NE(b,nullptr);
  EXPECT_EQ(b, bn);
  EXPECT_EQ(b->l, 1);
  EXPECT_EQ(b->u, 2);
  EXPECT_EQ(b->c, 2);
  EXPECT_EQ(b->left, bp);
 
  bins[1] = b;
  bp = b;
  bn = b->right;

  b = hv.getBinByIndex(2);
  ASSERT_NE(b,nullptr);
  EXPECT_EQ(b, bn);
  EXPECT_EQ(b->l, 2);
  EXPECT_EQ(b->u, 3);
  EXPECT_EQ(b->c, 3);
  EXPECT_EQ(b->left, bp);
  ASSERT_NE(b->right, nullptr);
  EXPECT_TRUE(b->right->is_end);
  
  bins[2] = b;

  //Test getBin
  EXPECT_EQ(hv.getBin(0.5),bins[0]);
  EXPECT_EQ(hv.getBin(1.5),bins[1]);
  EXPECT_EQ(hv.getBin(2.5),bins[2]);

  EXPECT_EQ(hv.getBin(0.0),nullptr);
  EXPECT_EQ(hv.getBin(3.01),nullptr);


  EXPECT_NEAR(hv.totalCount(),6.0,1e-8);

}



TEST(TestHistogramVBW, extractUniformCountInRangeInt){
  typedef HistogramVBW::Bin Bin;

  //Check for a normal histogram
  {
    Histogram h;
    h.set_histogram({1,2,3}, 1e-12, 3, 0,1);
    
    //total sum
    {
      HistogramVBW hcp(h);
      EXPECT_EQ(hcp.extractUniformCountInRangeInt(0,3),6.);
      EXPECT_EQ(hcp.totalCount(), 0.);
    }

    //first bin
    {
      HistogramVBW hcp(h);
      EXPECT_EQ(hcp.extractUniformCountInRangeInt(0,1),1.);
      EXPECT_EQ(hcp.Nbin(),3);
      EXPECT_EQ(hcp.getBinByIndex(0)->c, 0.);
    }

    //second bin
    {
      HistogramVBW hcp(h);
      EXPECT_EQ(hcp.extractUniformCountInRangeInt(1,2),2.);
      EXPECT_EQ(hcp.Nbin(),3);
      EXPECT_EQ(hcp.getBinByIndex(1)->c, 0.);
    }

    //last bin
    {
      HistogramVBW hcp(h);
      EXPECT_EQ(hcp.extractUniformCountInRangeInt(2,3),3.);
      EXPECT_EQ(hcp.Nbin(),3);
      EXPECT_EQ(hcp.getBinByIndex(2)->c, 0.);
    }

    //range left of histogram
    {
      HistogramVBW hcp(h);
      EXPECT_EQ(hcp.extractUniformCountInRangeInt(-1,0),0.);
      EXPECT_EQ(hcp.Nbin(),3);
    }

    //range right of histogram
    {
      HistogramVBW hcp(h);
      EXPECT_EQ(hcp.extractUniformCountInRangeInt(3.01,4),0.);
      EXPECT_EQ(hcp.Nbin(),3);
    }

    //range beyond but includes histogram
    {
      HistogramVBW hcp(h);
      EXPECT_EQ(hcp.extractUniformCountInRangeInt(-1,4),6.);
      EXPECT_EQ(hcp.totalCount(), 0.);
    }

    //lower bound matches upper edge of histogram
    {
      HistogramVBW hcp(h);
      EXPECT_EQ(hcp.extractUniformCountInRangeInt(3,4),0.);
      EXPECT_EQ(hcp.totalCount(), 6.);
    }

    //partial bin
    {
      HistogramVBW hcp(h);
      EXPECT_EQ(hcp.Nbin(),3);	    
      EXPECT_EQ(hcp.extractUniformCountInRangeInt(2.5,3),2.0);
      ASSERT_EQ(hcp.Nbin(),4);

      Bin const* lsplit = hcp.getBinByIndex(2);
      EXPECT_EQ(lsplit->c, 1.0);
      EXPECT_EQ(lsplit->l, 2.0);
      EXPECT_EQ(lsplit->u, 2.5);

      Bin const* rsplit = hcp.getBinByIndex(3);
      EXPECT_EQ(rsplit->c, 0.0);
      EXPECT_EQ(rsplit->l, 2.5);
      EXPECT_EQ(rsplit->u, 3.0);

      EXPECT_EQ(lsplit->right, rsplit);
      EXPECT_EQ(rsplit->left, lsplit);
    }
    
    //multiple partial bins
    {
      HistogramVBW hcp(h);	          
      EXPECT_EQ(hcp.Nbin(),3);	    
      EXPECT_EQ(hcp.extractUniformCountInRangeInt(0.5,2.5), 1+2+1);
      ASSERT_EQ(hcp.Nbin(),5);

      Bin const* lsplitl = hcp.getBinByIndex(0);
      EXPECT_EQ(lsplitl->c, 0.0);
      EXPECT_EQ(lsplitl->l, 0.0);
      EXPECT_EQ(lsplitl->u, 0.5);

      Bin const* lsplitr = hcp.getBinByIndex(1);
      EXPECT_EQ(lsplitr->c, 0.0);
      EXPECT_EQ(lsplitr->l, 0.5);
      EXPECT_EQ(lsplitr->u, 1.0);
      
      EXPECT_EQ(lsplitl->right, lsplitr);
      EXPECT_EQ(lsplitr->left, lsplitl);

      Bin const* b = hcp.getBinByIndex(2);
      EXPECT_EQ(b->c, 0.);

      Bin const* rsplitl = hcp.getBinByIndex(3);
      EXPECT_EQ(rsplitl->c, 0.0);
      EXPECT_EQ(rsplitl->l, 2.0);
      EXPECT_EQ(rsplitl->u, 2.5);

      Bin const* rsplitr = hcp.getBinByIndex(4);
      EXPECT_EQ(rsplitr->c, 2.0);
      EXPECT_EQ(rsplitr->l, 2.5);
      EXPECT_EQ(rsplitr->u, 3.0);

      EXPECT_EQ(rsplitl->right, rsplitr);
      EXPECT_EQ(rsplitr->left, rsplitl);
    }
      
  }
  //Check special treatment for a histogram with min=max
  {
    Histogram h;
    h.set_histogram({3}, 0.5, 0.5, 0.5-1e-8,1e-8);

    {
      HistogramVBW hcp(h);	    
      EXPECT_EQ(hcp.Nbin(),1);
      EXPECT_EQ(hcp.extractUniformCountInRangeInt(0.499,0.5),3.);
      EXPECT_EQ(hcp.Nbin(),1);
      EXPECT_EQ(hcp.getBinByIndex(0)->c,0.);
    }
    {
      HistogramVBW hcp(h);
      EXPECT_EQ(hcp.Nbin(),1);	                
      EXPECT_EQ(hcp.extractUniformCountInRangeInt(0.5,0.501),0.);
      EXPECT_EQ(hcp.Nbin(),1);
      EXPECT_EQ(hcp.getBinByIndex(0)->c,3.);
    }
  }

}


struct HistogramTest: public Histogram{
  //using Histogram::merge_histograms_uniform;
  using Histogram::merge_histograms_uniform_int;

};

TEST(TestHistogram, mergeUniformInt){
  //Test merge when bin edges align
  {
    HistogramTest h;
    h.set_histogram({0,0,0}, 1e-8,3, 0,1);
    
    HistogramTest l;
    l.set_histogram({0,3,0}, 1e-8,3, 0,1);

    HistogramTest g;
    g.set_histogram({2,0,1}, 1e-8,3, 0,1);

    HistogramTest::merge_histograms_uniform_int(h,l,g);

    EXPECT_EQ(h.binCount(0),2);
    EXPECT_EQ(h.binCount(1),3);
    EXPECT_EQ(h.binCount(2),1);
    std::cout << "Passed test when edges align" << std::endl;
  }

  //Test merge when bin edges don't align
  {
    HistogramTest h;
    h.set_histogram({0,0,0,0,0,0}, -1+1e-8,5, -1,1);
    
    HistogramTest l;
    l.set_histogram({4,0,0}, -1+1e-8,5, -1,2);

    HistogramTest g;
    g.set_histogram({6,0}, -1+1e-8,3, -1,2);

    std::cout << "Merging l:\n" << l << std::endl;
    std::cout << "Merging g:\n" << g << std::endl;
    std::cout << "Into l:\n" << h << std::endl;

    HistogramTest::merge_histograms_uniform_int(h,g,l);

    EXPECT_EQ(h.binCount(0),5); //expect 1/2 of the first bin from each
    EXPECT_EQ(h.binCount(1),5); //same for the second bin

    for(int i=2;i<6;i++) EXPECT_EQ(h.binCount(i),0);
    std::cout << "Passed test when edges don't align" << std::endl;

    EXPECT_EQ(h.totalCount(),10.0);
  }


  //Test with fractional counts
  {
    HistogramTest h;
    h.set_histogram({0,0,0,0,0,0,0,0}, -1+1e-8,3,  -1,0.5);
    
    HistogramTest l;
    l.set_histogram({3,0}, -1+1e-8,3, -1,2);

    HistogramTest g;
    g.set_histogram({0,2}, -1+1e-8,3, -1,2);

    HistogramTest::merge_histograms_uniform_int(h,l,g);

    EXPECT_EQ(h.totalCount(),5.0);

    std::cout << "Merged: " << h << std::endl;

    //-1:1 gets split into 4 parts
    //For l,  first split  -1:1 to -1:-0.5  -0.5:1   ratio  1/4:3/4    initial counts 3/4:9/4 ,  rounded 1:2 no defecit
    //        second split  -0.5:1  to  -0.5:0 and 0:1   ratio  1/3:2/3   initial counts 2/3,4/3  rounded 1:1 no defecit
    //        third split   0:1 to 0:0.5  0.5:1   ratio 1/2:1/2  initial counts  1/2:1/2  rounded 1:1    0:1 after defecit
    EXPECT_EQ(h.binCount(0),1);
    EXPECT_EQ(h.binCount(1),1);
    EXPECT_EQ(h.binCount(2),0);
    EXPECT_EQ(h.binCount(3),1);

    //1:3 gets split into 4 parts
    //For g, first split 1:3 to 1:1.5 and 1.5:3  ratio 1/4:3/4  initial counts 2/4:6/4    rounded  1:2  1:1 after defecit
    //       second split 1.5:3 to  1.5:2  and 2:3  ratio 1/3:2/3  initial counts 1/3:2/3  rounded 0:1 no defecit
    //       third split  2:3 to 2:2.5 and  2.5:3 ratio 1/2:1/2  initial counts 1/2:1/2 rounded 1:1   0:1 after defecit
    EXPECT_EQ(h.binCount(4),1);
    EXPECT_EQ(h.binCount(5),0);
    EXPECT_EQ(h.binCount(6),0);
    EXPECT_EQ(h.binCount(7),1);
  }
}

//Test the correction used for COPOD where we shift the bin edges such that the CDF of the min value is 1/N rather than 0
TEST(TestHistogram, empiricalCDFshift){
  Histogram g;
  g.set_histogram({2,6,5}, 0.1+1e-8,0.4, 0.1,0.1);

  //Check initial CDF is ~0
  double min = g.getMin();
  EXPECT_NEAR(g.empiricalCDF(min), 0., 1e-4);

  double n = g.totalCount();
  
  //Require  (min - edge)/bin_width * bin_count[0] / n = 1./n
  //edge = min - bin_width/bin_count[0]
  double shift = -0.1/2;
  g.set_histogram({2,6,5}, 0.1+1e-8+shift,0.4+shift, 0.1+shift,0.1);

  double cdf = g.empiricalCDF(min);
  double desire = 1./n;
  std::cout << min << " " << cdf << " desire " << desire << std::endl;
  EXPECT_NEAR(cdf , desire, 1e-6);
}

TEST(TestHistogram, maxNbinBinWidthSpecifiers){
  std::vector<double> runtimes = { 1,2,3,4,5,6,7,8,9,10 };

  //Test that the bin width specifiers that enforce a maximum number of bins are working
  {
    binWidthFixedMaxNbin bw1(0.1, 200);  
    EXPECT_EQ(bw1.bin_width(runtimes,1,10), 0.1);
    Histogram h(runtimes, bw1);
    
    EXPECT_EQ(h.Nbin(),91);
    EXPECT_NEAR(h.binWidth(),0.1,1e-2);   //it will modify the bin width to max the max sit exactly on the upper bin edge
  }

  {
    binWidthFixedMaxNbin bw1(0.1, 50);  
    EXPECT_NEAR(bw1.bin_width(runtimes,1,10), (10-0.99999)/50, 1e-2 );
    Histogram h(runtimes, bw1);
    
    EXPECT_EQ(h.Nbin(),50);
    EXPECT_NEAR(h.binWidth(), (10-0.99999)/50,1e-2);   //it will modify the bin width to max the max sit exactly on the upper bin edge
  }

  {
    binWidthScott bws;  
    binWidthScottMaxNbin bwsf(200);

    EXPECT_EQ(bws.bin_width(runtimes,1,10), bwsf.bin_width(runtimes,1,10) );
    Histogram hs(runtimes, bws);
    Histogram hsf(runtimes, bwsf);
    
    EXPECT_EQ(hs,hsf);
  }

  {
    binWidthScott bws;  
    binWidthScottMaxNbin bwsf(1);

    EXPECT_NE(bws.bin_width(runtimes,1,10), bwsf.bin_width(runtimes,1,10) );
    Histogram hs(runtimes, bws);
    Histogram hsf(runtimes, bwsf);
    
    EXPECT_EQ(hsf.Nbin(),1);
    EXPECT_NE(hs,hsf);    
  }
}

TEST(TestHistogramVBW, extractUniformCountInRangeIntArray){
  typedef HistogramVBW::Bin Bin;

  //Check for a normal histogram
  {
    Histogram h;
    h.set_histogram({7,21,33}, 1e-12, 3, 0,1); //count, min, max, start, binwidth

    std::cout << h << std::endl;
    std::vector<std::pair<double,double> > edges = {
      { -1,-0.001 },
      { -0.001, 0.5 },
      { 0.5, 2} ,
      { 2, 2.85}, 
      { 2.85, 3.0},
      { 3.0, 4.8 },
      { 4.8, 9}
    };

    HistogramVBW v1(h);
    std::cout << "Init VBW:" << std::endl << v1 << std::endl;

    std::vector<double> expect;
    for(auto const &e: edges){
      double v = v1.extractUniformCountInRangeInt(e.first,e.second);
      std::cout << "Extract " << e.first << "-" << e.second << " got " << v << std::endl;    
      expect.push_back(v);
      std::cout << "New VBW:" << std::endl << v1 << std::endl;
    }
    
    HistogramVBW v2(h);
    std::vector<double> got = v2.extractUniformCountInRangesInt(edges);
    
    EXPECT_EQ(expect.size(),got.size());

    size_t i=0;
    for(auto const &e: edges){
      std::cout << e.first << "-" << e.second << " got " << got[i] << " expect " << expect[i] << std::endl;
      EXPECT_NEAR(expect[i],got[i],1e-8);
      i++;
    }
  }


  //Check special treatment for a histogram with min=max
  {
    Histogram h;
    h.set_histogram({33}, 0.5, 0.5, 0.5-1e-8,1e-8);

    std::vector<std::pair<double,double> > edges = {
      { -1,-0.499 },
      { 0.499, 0.5 },
      { 0.5, 3.0 }
    };
    std::cout << h << std::endl;

    HistogramVBW v1(h);
    std::cout << "Init VBW:" << std::endl << v1 << std::endl;

    std::vector<double> expect;
    for(auto const &e: edges){
      double v = v1.extractUniformCountInRangeInt(e.first,e.second);
      std::cout << "Extract " << e.first << "-" << e.second << " got " << v << std::endl;    
      expect.push_back(v);
      std::cout << "New VBW:" << std::endl << v1 << std::endl;
    }
    
    HistogramVBW v2(h);
    std::vector<double> got = v2.extractUniformCountInRangesInt(edges);
    
    EXPECT_EQ(expect.size(),got.size());

    size_t i=0;
    for(auto const &e: edges){
      std::cout << e.first << "-" << e.second << " got " << got[i] << " expect " << expect[i] << std::endl;
      EXPECT_NEAR(expect[i],got[i],1e-8);
      i++;
    }
  }


}


TEST(TestHistogram, serialize){
  Histogram g;
  g.set_histogram({2,40,24,10,3,1,0,1}, 0.1+1e-8,0.9,  0.1,0.1);

  {
    std::stringstream ss;
    {
      cereal::PortableBinaryOutputArchive wr(ss);
      wr(g);
    }
    
    Histogram h;
    {
      cereal::PortableBinaryInputArchive rd(ss);
      rd(h);
    }
    EXPECT_EQ(g,h);
    EXPECT_EQ(h.Nbin(),g.Nbin());
  }

  std::unordered_map<int, Histogram> gg = { {123, g} };
  {
    std::stringstream ss;
    {
      cereal::PortableBinaryOutputArchive wr(ss);
      wr(gg);
    }
    
    std::unordered_map<int, Histogram> hh;
    {
      cereal::PortableBinaryInputArchive rd(ss);
      rd(hh);
    }
    EXPECT_EQ(gg,hh);
  }
 

}

   
  
