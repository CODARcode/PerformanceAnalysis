//This code uses the NAB data sets https://github.com/numenta/NAB  to evaluate our AD algorithms
#include<iostream>
#include<sstream>
#include<cassert>
#include<vector>
#include<fstream>
#include<unordered_map>
#include<nlohmann/json.hpp>
#include<chimbuko/chimbuko.hpp>
#include<chimbuko/util/Histogram.hpp>
#include<chimbuko/util/error.hpp>
#include<chimbuko/util/commandLineParser.hpp>

using namespace chimbuko;

struct DataPoint{
  size_t idx; //for tracking
  std::string tag;
  double value;
  bool is_outlier;

  DataPoint(const std::string &tag, const double value): tag(tag), value(value), is_outlier(false){
    static size_t q = 0;
    idx = q++;
  }

  //ExecData data type is long int, not double. We therefore multiply the values by 10^10 before downcasting to avoid truncation
  //ExecData is also used expecting non-negative positive values. We therefore allow for a shift and assert it produces a non-negative result
  ExecData_t toExecData(double shift) const;
};

std::ostream & operator<<(std::ostream &os, const DataPoint &d){
  os << "(" << d.idx << "," << d.tag << "," << d.value << "," << (int)d.is_outlier << ")";
  return os;
}

unsigned long transform_to_internal(double v, double shift){
  long itime(1e10 * (v+shift) );
  if(itime < 0) assert(0 && "Shift is not sufficient to produce positive values");
  return itime;
}
double transform_from_internal(unsigned long v, double shift){
  double d(v);
  d /= 1e10;
  d -= shift;
  return d;
}


ExecData_t DataPoint::toExecData(double shift) const{
  ExecData_t out(eventID(0,0,idx), 0,0,0,0,"fname", 0, transform_to_internal(value,shift)); 
  return out;
}



//Determine the shift appropriate to make all values non-negative
double getShift(const std::vector<DataPoint> &v){
  double shift = 0;
  for(auto const &d : v){
    if(d.value < 0.) shift = std::max(shift, -d.value);
  }
  return shift;
}

CallList_t generateCallList(const std::vector<DataPoint> &v, double shift){
  CallList_t out;
  for(auto const &d: v)
    out.push_back(d.toExecData(shift));
  return out;
}

//Break up the data set into some number of batches
//Also removes any existing labels on the data
std::vector<ExecDataMap_t> generateBatches(CallList_t &data, int Nbatch){
  size_t n(ceil(double(data.size()) / Nbatch));
  size_t i = 0;
  std::vector<ExecDataMap_t> out(Nbatch, ExecDataMap_t({ {0, std::vector<CallListIterator_t>()} }));
  for(auto it = data.begin(); it != data.end(); ++it){
    it->set_label(0); //unset existing labeling if any
    int batch = i / n;
    assert(batch < Nbatch);
    out[batch][0].push_back(it);
    i++;
  }
  size_t tsize = 0;
  std::cout << "Generated " << Nbatch << " batches of sizes: ";
  for(auto &b : out){
    size_t sz = b[0].size(); 
    tsize += sz;
    std::cout << sz << " ";
  }
  std::cout << std::endl;
  assert(tsize == data.size());
  
  return out;
}



  
//Parse the csv files containing the data
std::vector<DataPoint> readData(const std::string &file){
  std::vector<DataPoint> out;
  std::ifstream in(file.c_str());
  if(!in.good()) return out;
  
  int lineno = 0;
  std::string line;

  while (std::getline(in,line)) {
    std::stringstream ss(line);
    std::string a, b;
    std::getline(ss, a, ',');
    std::getline(ss, b);
    assert(a.size() > 0);
    assert(b.size() > 0);

    if(lineno == 0){
      if(a != "timestamp") assert(0 && "Expected 'timestamp' for first string");
      if(b != "value") assert(0 && "Expected 'value' for second string");
    }else{      
      std::stringstream v(b);
      double vv; v>>vv;
      DataPoint d(a,vv);
      out.push_back(d);
      std::cout << d << std::endl;
    }
    lineno++;
  }
  std::cout << "Read " << out.size() << " values" << std::endl;

  return out;
}

void readLabels(std::vector<DataPoint> &d, const std::string &dataset_tag, const std::string &file){
  std::unordered_map<std::string, DataPoint*> dm;
  for(auto &dd: d) dm[dd.tag] = &dd;
  
  std::ifstream in(file);
  nlohmann::json labels;
  in >> labels;

  if(!labels.contains(dataset_tag)) assert(0 && "Could not find dataset in labels file");
  auto const &v = labels[dataset_tag];
  if(!v.is_array()) assert(0 && "Labels are not in an array!");

  int noutlier = 0;
  for(const std::string &outlier : v){
    auto it = dm.find(outlier);
    if(it == dm.end()) assert(0 && "Could not find data point in map");
    DataPoint &into = *it->second;
    into.is_outlier = true;
    std::cout << "Outlier " << into << std::endl;
    ++noutlier;
  }
  std::cout << "Labeled " << noutlier << " outliers" << std::endl;
}

void assignLabelsHistogram(std::vector<DataPoint> &data, double outlier_threshold){
  std::vector<double> hdata(data.size());
  size_t i=0;
  for(auto const &d : data) hdata[i++] = d.value;
  
  Histogram h(hdata);
    
  //Get max prob
  double hsum = h.totalCount();
  std::vector<double> probs(h.Nbin());
  double max_prob = 0;
  for(int b=0;b<h.Nbin();b++){
    probs[b] = h.binCount(b) / hsum;
    max_prob = std::max(probs[b], max_prob);
  }

  //Label outliers
  size_t noutlier = 0;
  for(auto &d : data){
    int bin = h.getBin(d.value);
    double prob = (bin == Histogram::LeftOfHistogram || bin == Histogram::RightOfHistogram) ? 0 : probs[bin];
    
    if(prob / max_prob < outlier_threshold){
      d.is_outlier = true;
      ++noutlier;
    }    
  }
  std::cout << "Labeled " << noutlier << " outliers using histogram method with threshold " << outlier_threshold << std::endl;
}

void evaluate(std::vector<std::pair<double,double> > &false_pos,
	      std::vector<std::pair<double,double> > &false_neg,
	      std::vector<std::pair<double,double> > &true_pos,
	      std::vector<std::pair<double,double> > &true_neg,
	      const CallList_t &call_list, const std::unordered_map<size_t, DataPoint const*> &data_idx_map){
  int nfalse_positive = 0;
  int nfalse_negative = 0;
  int ntrue_positive = 0;
  int ntrue_negative = 0;

  //Also output the data in a form suitable for plotting 
  for(auto const &d : call_list){
    auto dpit = data_idx_map.find(d.get_id().idx);
    if(dpit == data_idx_map.end()) assert(0 && "Could not find DataPoint matching index");
    const DataPoint &dp = *dpit->second;
    
    if(d.get_label() == 0) assert(0 && "Encountered unlabeled data point");

    bool ad_is_outlier = d.get_label() == -1;
    bool is_actually_outlier =  dp.is_outlier;

    std::string descr;
    if(ad_is_outlier && is_actually_outlier){
      descr = "true positive";
      ++ntrue_positive;
      true_pos.push_back({d.get_id().idx, dp.value});
    }else if(ad_is_outlier && !is_actually_outlier){
      descr = "FALSE positive";
      ++nfalse_positive;
      false_pos.push_back({d.get_id().idx, dp.value});
    }else if(!ad_is_outlier && is_actually_outlier){
      descr = "FALSE negative";
      ++nfalse_negative;
      false_neg.push_back({d.get_id().idx, dp.value});
    }else if(!ad_is_outlier && !is_actually_outlier){
      descr = "true negative";
      ++ntrue_negative;
      true_neg.push_back({d.get_id().idx, dp.value});
    }
    std::cout << dp << " ival " << d.get_runtime() << " " << descr << std::endl;
  }

  std::cout << "True positive: " << ntrue_positive << std::endl;
  std::cout << "True negative: " << ntrue_negative << std::endl;
  std::cout << "FALSE positive: " << nfalse_positive << std::endl;
  std::cout << "FALSE negative: " << nfalse_negative << std::endl;
}



void evaluate(int &nfalse_positive, int &nfalse_negative, int &ntrue_positive, int &ntrue_negative,
	      const CallList_t &call_list, const std::unordered_map<size_t, DataPoint const*> &data_idx_map,
	      bool verbose = false){
  nfalse_positive = 0;
  nfalse_negative = 0;
  ntrue_positive = 0;
  ntrue_negative = 0;

  for(auto const &d : call_list){
    auto dpit = data_idx_map.find(d.get_id().idx);
    if(dpit == data_idx_map.end()) assert(0 && "Could not find DataPoint matching index");
    const DataPoint &dp = *dpit->second;
    
    if(d.get_label() == 0) assert(0 && "Encountered unlabeled data point");

    bool ad_is_outlier = d.get_label() == -1;
    bool is_actually_outlier =  dp.is_outlier;

    std::string descr;
    if(ad_is_outlier && is_actually_outlier){
      descr = "true positive";
      ++ntrue_positive;
    }else if(ad_is_outlier && !is_actually_outlier){
      descr = "FALSE positive";
      ++nfalse_positive;
    }else if(!ad_is_outlier && is_actually_outlier){
      descr = "FALSE negative";
      ++nfalse_negative;
    }else if(!ad_is_outlier && !is_actually_outlier){
      descr = "true negative";
      ++ntrue_negative;
    }
    if(verbose) std::cout << dp << " ival " << d.get_runtime() << " " << descr << std::endl;
  }

  if(verbose){
    std::cout << "True positive: " << ntrue_positive << std::endl;
    std::cout << "True negative: " << ntrue_negative << std::endl;
    std::cout << "FALSE positive: " << nfalse_positive << std::endl;
    std::cout << "FALSE negative: " << nfalse_negative << std::endl;
  }
}



void printFullDataHistogram(const CallList_t &call_list, const std::unordered_map<size_t, DataPoint const*> &data_idx_map, double shift){
  std::vector<double> hdata(call_list.size());
  size_t i=0;
  for(auto v : call_list) hdata[i++] = v.get_runtime();
  Histogram h(hdata);
    
  double hsum = h.totalCount();
  std::vector<double> probs(h.Nbin());
  double max_prob = 0;
  for(int b=0;b<h.Nbin();b++){
    probs[b] = h.binCount(b) / hsum;
    max_prob = std::max(probs[b], max_prob);
  }
  std::vector<int> true_outlier_counts(h.Nbin(), 0);
  for(auto v: call_list){
    int bin = h.getBin(v.get_runtime());
    auto dpit = data_idx_map.find(v.get_id().idx);
    if(dpit == data_idx_map.end()) assert(0 && "Could not find DataPoint matching index");
    const DataPoint &dp = *dpit->second;
      
    if(dp.is_outlier) ++true_outlier_counts[bin];
  }
      
  std::cout << "Full dataset histogram:" << std::endl;
  for(int b=0;b<h.Nbin();b++){
    double ledge = transform_from_internal(h.bin_edges()[b],shift);
    double uedge = transform_from_internal(h.bin_edges()[b+1],shift);

    std::cout << "Bin " << b << " edges (" << ledge << "," << uedge << ") count " << h.binCount(b) << " bin prob " << probs[b] << " prob relative to peak " << probs[b]/max_prob << " #true outliers " << true_outlier_counts[b] << std::endl;
  }
}

void printEvaluations(const std::vector<std::pair<double,double> > &false_pos,
		      const std::vector<std::pair<double,double> > &false_neg,
		      const std::vector<std::pair<double,double> > &true_pos,
		      const std::vector<std::pair<double,double> > &true_neg
		      ){

  std::ofstream f("results.dat");
  for(auto const &v : true_neg) f << v.first << " " << v.second << std::endl;
  f << std::endl;
  for(auto const &v : true_pos) f << v.first << " " << v.second << std::endl;
  f << std::endl;
  for(auto const &v : false_neg) f << v.first << " " << v.second << std::endl;
  f << std::endl;
  for(auto const &v : false_pos) f << v.first << " " << v.second << std::endl;
}

      
void printXMGrace(const std::vector<std::pair<double,double> > &false_pos,
		  const std::vector<std::pair<double,double> > &false_neg,
		  const std::vector<std::pair<double,double> > &true_pos,
		  const std::vector<std::pair<double,double> > &true_neg
		  ){

  std::ofstream f("results.agr"); //XMGRACE format
  f << R"(@    s0 hidden false
@    s0 type xy
@    s0 symbol 1
@    s0 symbol size 0.030000
@    s0 symbol color 1
@    s0 symbol pattern 1
@    s0 symbol fill color 1
@    s0 symbol fill pattern 0
@    s0 symbol linewidth 1.0
@    s0 symbol linestyle 1
@    s0 symbol char 65
@    s0 symbol char font 0
@    s0 symbol skip 0
@    s0 line type 0
@    s0 legend  "true neg"
@target G0.S0
@type xy
)";
  for(auto const &v : true_neg) f << v.first << " " << v.second << std::endl;
  f << "&" << std::endl;
  f << R"(@    s1 hidden false
@    s1 type xy
@    s1 symbol 1
@    s1 symbol size 0.100000
@    s1 symbol color 2
@    s1 symbol pattern 1
@    s1 symbol fill color 2
@    s1 symbol fill pattern 0
@    s1 symbol linewidth 1.0
@    s1 symbol linestyle 1
@    s1 symbol char 65
@    s1 symbol char font 0
@    s1 symbol skip 0
@    s1 line type 0
@    s1 legend  "true pos"
@target G0.S1
@type xy
)";
  for(auto const &v : true_pos) f << v.first << " " << v.second << std::endl;
  f << "&" << std::endl;
  f << R"(@    s2 hidden false
@    s2 type xy
@    s2 symbol 2
@    s2 symbol size 0.100000
@    s2 symbol color 4
@    s2 symbol pattern 1
@    s2 symbol fill color 4
@    s2 symbol fill pattern 0
@    s2 symbol linewidth 1.0
@    s2 symbol linestyle 1
@    s2 symbol char 65
@    s2 symbol char font 0
@    s2 symbol skip 0
@    s2 line type 0
@    s2 legend  "false neg"
@target G0.S2
@type xy
)";
  for(auto const &v : false_neg) f << v.first << " " << v.second << std::endl;
  f << "&" << std::endl;
  f << R"(@    s3 hidden false
@    s3 type xy
@    s3 symbol 3
@    s3 symbol size 0.100000
@    s3 symbol color 4
@    s3 symbol pattern 1
@    s3 symbol fill color 4
@    s3 symbol fill pattern 0
@    s3 symbol linewidth 1.0
@    s3 symbol linestyle 1
@    s3 symbol char 65
@    s3 symbol char font 0
@    s3 symbol skip 0
@    s3 line type 0
@    s3 legend  "false pos"
@target G0.S3
@type xy
)";
  for(auto const &v : false_pos) f << v.first << " " << v.second << std::endl;
  f << "&" << std::endl;
}


struct metrics{
  double threshold;
  double TPR; //true positive rate (aka sensitivity)
  double TNR; //true negative rate (aka specificity)
  double FPR; //false positive rate

  metrics() = default;
  metrics(double _threshold, int nfalse_positive, int nfalse_negative, int ntrue_positive, int ntrue_negative){ compute(threshold,nfalse_positive, nfalse_negative, ntrue_positive, ntrue_negative); }

  void compute(double _threshold, int nfalse_positive, int nfalse_negative, int ntrue_positive, int ntrue_negative){
    threshold = _threshold;
    TPR = double(ntrue_positive)/( ntrue_positive + nfalse_negative );
    TNR = double(ntrue_negative)/( ntrue_negative + nfalse_positive );
    FPR = 1.-TNR;
  }

};

double computeAUC(std::vector<metrics> m){
  //Sort by FPR
  std::sort(m.begin(),m.end(),[](const metrics &a, const metrics &b){ return a.FPR < b.FPR; });
  double auc = 0;
  for(size_t i=0;i<m.size()-1;i++)
    auc += ( m[i+1].FPR - m[i].FPR ) * ( m[i+1].TPR + m[i].TPR ) / 2. ; //trapezoid rule
  return auc;
}



struct Args{
  std::string filename; //filename of NAB .csv file
  double label_outlier_threshold; //in labeling, relative bin probability to largest bin must be smaller than this value

  int nbatch; //number of data batches
  double ad_outlier_threshold; //outlier algorithm threshold
  int maxbins;

  Args(){
    label_outlier_threshold = 0.01;
    nbatch=1;
    ad_outlier_threshold = 0.99;
    maxbins=100;
  }
};


int main(int argc, char **argv){
  //Command line args
  commandLineParser<Args> parser;
  
  addMandatoryCommandLineArg(parser, filename, "Filename of the NAB .csv data");
  addOptionalCommandLineArg(parser, label_outlier_threshold, "In labeling, relative bin probability to largest bin must be smaller than this value");
  addOptionalCommandLineArg(parser, nbatch, "Number of data batches");
  addOptionalCommandLineArg(parser, ad_outlier_threshold, "Outlier algorithm threshold");
  addOptionalCommandLineArg(parser, maxbins, "Maximum number of bins in AD algorithm histograms");
  
  Args args;
  parser.parseCmdLineArgs(args, argc, argv);

  std::vector<DataPoint> data = readData(args.filename);
  
  //Get the data labels
  //readLabels(data, argv[2], argv[3]);
  //   NOTE: It looks like the NAB labels are local outliers in time series data, not global outliers. This is not relevant for Chimbuko
  //         Instead we will generate a histogram of the data set and define outliers based on the relative bin probability to the peak bin
  //         While this is not as good as hand-labeled outliers, it will allow us to test batch size dependence and any errors in the AD algorithm
  assignLabelsHistogram(data, args.label_outlier_threshold);
  
  //Get the shift to ensure all values positive
  double shift = getShift(data);
  std::cout << "Determined base shift " << shift << std::endl;

  CallList_t call_list = generateCallList(data,shift); //std::list<ExecData_t>

  std::unordered_map<size_t, DataPoint const*> data_idx_map; //map of data pointindex to the datapoint so we can match up data from the call list to the original data
  for(auto const &d : data) data_idx_map[d.idx] = &d;

  //Get the histogram of the whole data set for comparison
  printFullDataHistogram(call_list, data_idx_map, shift);

  std::vector<ExecDataMap_t> batches = generateBatches(call_list, args.nbatch);

  ADOutlier::OutlierStatistic stat = ADOutlier::ExclusiveRuntime;
  ADOutlierHBOS ad(stat, args.ad_outlier_threshold, true, args.maxbins);

  for(int b=0;b<args.nbatch;b++){
    ad.linkExecDataMap(&batches[b]);
    enableVerboseLogging() = true;
    ad.run(b);
    enableVerboseLogging() = false;
  }

  //Check results
  //Also output the data in a form suitable for plotting
  std::vector<std::pair<double,double> > false_pos;
  std::vector<std::pair<double,double> > false_neg;
  std::vector<std::pair<double,double> > true_pos;
  std::vector<std::pair<double,double> > true_neg;
  evaluate(false_pos,false_neg,true_pos,true_neg,call_list,data_idx_map);
  printEvaluations(false_pos,false_neg,true_pos,true_neg);
  printXMGrace(false_pos,false_neg,true_pos,true_neg);

  std::vector<metrics> ROC;

  std::cout << "Starting ROC analysis" << std::endl;
  for(int i=0;i<100;i++){
    for(ExecData_t &c: call_list) c.set_label(0);

    double thres = i*0.01;
    ad.set_threshold(i*0.01);

    for(int b=0;b<args.nbatch;b++){
      ad.linkExecDataMap(&batches[b]);
      ad.run(b);
    }
    
    int nfalse_positive = 0;
    int nfalse_negative = 0;
    int ntrue_positive = 0;
    int ntrue_negative = 0;

    evaluate(nfalse_positive,nfalse_negative,ntrue_positive,ntrue_negative,call_list,data_idx_map);
    
    metrics m(thres, nfalse_positive, nfalse_negative, ntrue_positive, ntrue_negative);
    std::cout << "thres=" << thres << " FP=" << nfalse_positive << " FN=" << nfalse_negative << " TP=" << ntrue_positive << " TN=" << ntrue_negative << " FPR " << m.FPR << " TPR " << m.TPR << std::endl;
    ROC.push_back(m);
  }
  std::cout << "AUC: " << computeAUC(ROC) << std::endl;

  return 0;
}
