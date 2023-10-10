#include<random>
#include<chimbuko/util/Histogram.hpp>
#include<chimbuko/verbose.hpp>
#include<chimbuko/pserver/PSparamManager.hpp>
#include<chimbuko/ad/ADNetClient.hpp>
#include<chimbuko/ad/ADOutlier.hpp>
#include<chimbuko/param/hbos_param.hpp>
#include<cassert>
#include<iostream>
#include<algorithm>
#include<limits>
#include <boost/math/distributions/normal.hpp>
#include <fstream>
#include "gtest/gtest.h"
#include<unit_test_common.hpp>

using namespace chimbuko;

struct Model{
  typedef boost::math::normal_distribution<double> normal;
  std::vector<double> mu;
  std::vector<double> sigma;
  std::vector<normal> dists;
  //\int_a^b dx p(x) = 1/N \sum_i \int_a^b dx p_i(x)

  Model(){}
  
  void addPeak(double _mu, double _sigma){
    mu.push_back(_mu);
    sigma.push_back(_sigma);
    dists.push_back(normal(_mu,_sigma));
  }


  double prob(double a, double b) const{
    double N = mu.size();
    double out = 0.;
    for(int i=0;i<mu.size();i++){
      out += boost::math::cdf(dists[i],b) - boost::math::cdf(dists[i],a);
    }
    out /= N;
    return out;
  }
  std::vector<double> sample(int N_per_dist, std::default_random_engine &gen){
    std::vector<double> out(N_per_dist * mu.size());
    
    int c=0;
    for(int i=0;i<mu.size();i++){
      std::normal_distribution<double> distribution(mu[i], sigma[i]);
      for(int n=0;n<N_per_dist;n++)
	out[c++] = distribution(gen);
    }
    return out;
  }
};


void plot_prob(const Histogram &g, const Model &m, const std::string &filename, bool verbose = false){
  double gsum = g.totalCount();
  
  std::vector<double> xvals(g.Nbin());
  std::vector<double> vest(g.Nbin());
  std::vector<double> vtrue(g.Nbin());

  for(int i=0;i<g.Nbin();i++){
    auto edges = g.binEdges(i);
    double prob = g.binCount(i)/gsum;
    double true_prob = m.prob(edges.first,edges.second);
    xvals[i] = g.binValue(i);
    vest[i] = prob;
    vtrue[i] = true_prob;
    double diff = fabs(prob-true_prob);
    std::cout << "Bin " << i << " range " << edges.first << "-" << edges.second << " est. prob. " << prob << " true prob. " << true_prob << " diff. " << diff << std::endl;
  }
  
  std::ofstream f(filename);
  for(int i=0;i<g.Nbin();i++)
    f << xvals[i] << " " << vest[i] << std::endl;
  f << std::endl;
  for(int i=0;i<g.Nbin();i++)
    f << xvals[i] << " " << vtrue[i] << std::endl;
  f.close();
}



struct DataSet{
  std::string m_legend;
  std::vector<double> m_x;
  std::vector<double> m_y;

  DataSet(const std::string &legend): m_legend(legend){}

  void add(const double x, const double y){
    m_x.push_back(x);
    m_y.push_back(y);
  }

  void toXMGRACE(std::ostream &os, const int index) const{
#define T(v) os << "@    s" << index << " " << v << std::endl
    T("hidden false");
    T("type xy");
    T("symbol 1");
    T("symbol size 0.060000");
    T("symbol color 1");
    T("symbol pattern 1");
    T("fill color 1");
    T("symbol fill pattern 0");
    T("symbol linewidth 1.0");
    T("symbol linestyle 1");
    T("symbol char 65");
    T("symbol char font 0");
    T("symbol skip 0");
    T("line type 0");
    os << "@    s" << index << " legend \"" << m_legend << "\"" << std::endl;
    os << "@target G0.S" << index << std::endl;
    os << "@type xy" << std::endl;

    for(int i=0;i<m_x.size();i++){
      os << m_x[i] << " " << m_y[i] << std::endl;
    }
    os << "&" << std::endl;
#undef T
  }


};


void lineXMGRACE(std::ostream &os, const double x0, const double y0, const double x1, const double y1){
#define T(v) os << "@    line " << v << std::endl
  os << "@with line" << std::endl;
  T("on");  
  T("loctype world");
  T("g0");
  os << "@    line " << x0 << ", " << y0 << ", " << x1 << ", " << y1 << std::endl;
  T("linewidth 1.5");
  T("linestyle 1");
  T("color 2");
  T("arrow 0");
  T("arrow type 0");
  T("arrow length 1.000000");
  T("arrow layout 1.000000, 1.000000");
  os << "@line def" << std::endl;
}


class TestNetClient: public ADNetClient{
public:
  int w; //current worker
  PSparamManager pm;   

  TestNetClient(const int nworker, const std::string &ad_alg): pm(nworker, ad_alg), w(0){
    m_use_ps = true;
  }

  void connect_ps(int rank, int srank = 0, std::string sname="MPINET") override{}
  void disconnect_ps() override{}

  using ADNetClient::send_and_receive;
    
  std::string send_and_receive(const Message &msg) override{
    std::cout << "TestNetClient send_and_receive updating worker model " << w << std::endl;
    Message response;
    response.set_msg( pm.updateWorkerModel(msg.buf(), w), false);
    w = (w + 1) % pm.nWorkers(); //round-robin
    return response.data();
  }
    
};


int main(int argc, char **argv){
  nlohmann::json params;

  if(argc < 2){
    params["runtime_s"] = 20000;
    params["nworker"] = 10;
    params["nclient"] = 1;
    params["funcs"] = nlohmann::json::array();
    params["do_full"] = true; //generate the full histogram from all data as well, and keep track of KL metric
    params["maxbins"] = 200;
    
    nlohmann::json f1 = { {"freq",100}, {"peaks", { {10000,100} } } }; //peaks is array of pairs of values, each corresponding to a peak and width 
    params["funcs"].push_back(f1);
    
    std::ofstream out("params.templ");
    out << params.dump(4) << std::endl;
    return 0;
  }
      
      
  //enableVerboseLogging() = true;
  std::default_random_engine gen;  gen.seed(1234);

  std::ifstream in(argv[1]);
  in >> params;
    
  int runtime_s = params["runtime_s"]; //200000; //5000;//50000;
  int nworker = params["nworker"];
  bool do_full = params["do_full"];
  int maxbins = params["maxbins"];
  
  std::string ad_alg = "hbos";
  TestNetClient nc(nworker,ad_alg);
  
  int nclient = params["nclient"];

  std::vector<ADOutlierHBOS> clients(nclient);

  for(int c=0;c<nclient;c++){
    clients[c].linkNetworkClient(&nc);
    clients[c].set_maxbins(maxbins);
  }

  int nfunc = params["funcs"].size();

  std::vector<Model> models(nfunc);
  std::vector<std::string> fnames(nfunc);
  std::vector<std::ofstream*> out_f_KL_ad(nfunc);
  std::vector<std::ofstream*> out_f_KL_full(nfunc);
  std::vector<int> npeak(nfunc);
  std::vector<int> func_freq(nfunc); //per second
  for(int i=0;i<nfunc;i++){
    auto const &f = params["funcs"][i];
    func_freq[i] = f["freq"];
    auto const &fp = f["peaks"];   
    npeak[i] = fp.size();
    
    std::cout << "Function " << i << " frequency " << func_freq[i] << ", peaks " << npeak[i] << " : ";
    for(int p=0;p<npeak[i];p++){
      assert(fp[p].size() == 2);
      int c = fp[p][0], w=fp[p][1];      
      models[i].addPeak(c,w);
      std::cout << "(" << c << "," << w << ")";
    }
    std::cout << std::endl;
    fnames[i] = std::string("func_") + std::to_string(i);
    out_f_KL_ad[i] = new std::ofstream(std::string("KL_ad_") + std::to_string(i));
    if(do_full) out_f_KL_full[i] = new std::ofstream(std::string("KL_full_") + std::to_string(i));    
  }

  //Store full data to allow comparison of histogram of full data set
  std::vector< std::vector<double> > fdata(nfunc);

  for(int s=0;s<runtime_s;s++){ //simulate second-long cycles
    std::cout << "Cycle " << s << std::endl;
    for(int c=0;c<nclient;c++){
      std::list< std::array<unsigned long, FUNC_EVENT_DIM> > events;   
      std::list<ExecData_t> calls;
      ExecDataMap_t em; //fid->vector of iterators to above
      clients[c].linkExecDataMap(&em);

      for(int f=0;f<nfunc;f++){
	std::vector<double> samples = models[f].sample(func_freq[f]/npeak[f], gen);
	for(double d : samples){
	  auto it = calls.insert(calls.end(), createFuncExecData_t(0,c,0,f,fnames[f],s*1000000,d,&events));
	  em[f].push_back(it);
	  if(do_full) fdata[f].push_back(d);
	}
      }
      clients[c].run(s);
    }
    nc.pm.updateGlobalModel(); //simulate ticking of merge cycle thread on PS

    //Evaluate
    HbosParam const *param = (HbosParam const*)nc.pm.getGlobalParamsPtr();
    auto const & pstats = param->get_hbosstats();

    assert(pstats.size() == nfunc);
    for(int f=0;f<nfunc;f++){
      assert(pstats.count(f) == 1);
      HbosFuncParam const & fp = pstats.find(f)->second;

      Histogram const &fh = fp.getHistogram();
      std::cout << "Function " << f << " bins " << fh.Nbin() << " count " << fh.totalCount() << " bin width " << fh.bin_edges()[1]-fh.bin_edges()[0] << " min " << fh.getMin() << " max " << fh.getMax() << std::endl;
      long tc = fh.totalCount();

      double KL_ad = 0;
      for(int b=0;b<fh.Nbin();b++){
	auto be = fh.binEdges(b);
	double prob = fh.binCount(b)/tc;
	double true_prob = models[f].prob(be.first,be.second);

	if(true_prob == 0) continue;
	
	if(prob != 0) KL_ad += true_prob * ( log10(true_prob) - log10(prob) );
      }
      (*out_f_KL_ad[f]) << s << " " << KL_ad << std::endl;
      
      double KL_full = 0;
      if(do_full){
	binWidthScottMaxNbin l_bwspec(maxbins);
	Histogram hist_full(fdata[f],l_bwspec);
	long tc_full = hist_full.totalCount();
	
	for(int b=0;b<hist_full.Nbin();b++){
	  auto be = hist_full.binEdges(b);
	  double prob = hist_full.binCount(b)/tc_full;
	  double true_prob = models[f].prob(be.first,be.second);

	  if(true_prob == 0) continue;
	
	  if(prob != 0) KL_full += true_prob * ( log10(true_prob) - log10(prob) );
	}	
	(*out_f_KL_full[f]) << s << " " << KL_full << std::endl;
      }
      
      std::cout << "KL divergence AD:" << KL_ad;
      if(do_full) std::cout << " full:" << KL_full;
      std::cout << std::endl;
    }
  }
  for(int f=0;f<nfunc;f++){
    delete out_f_KL_ad[f];
    if(do_full) delete out_f_KL_full[f];
  }

  return 0;
}
