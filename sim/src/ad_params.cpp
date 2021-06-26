#include <pybind11/pybind11.h>
#include<chimbuko/util/error.hpp>
#include <sim/ad_params.hpp>
#include<random>


using namespace chimbuko;
using namespace chimbuko_sim;

void chimbuko_sim::registerFunc(const std::string &func_name, unsigned long mean_runtime, unsigned long std_dev_runtime, int seen_count){
  if(adAlgorithmParams().algorithm != "none") fatal_error("This function should only be used if ad_algorithm()==\"none\"");
  unsigned long fidx = registerFunc(func_name);

  //Generate fake statistics
  RunStats &stats = globalParams()[fidx];
  static std::default_random_engine gen(1234);
  std::normal_distribution<double> dist(mean_runtime,std_dev_runtime);

  for(int i=0;i<seen_count;i++){
    unsigned long val = dist(gen);
    stats.push(val);
  }
  std::cout << "RunStats for func " << func_name << " : " << stats.get_json().dump() << std::endl;
}


namespace py = pybind11;

PYBIND11_MODULE(ad_param, m) {
  m.doc() = "PYBIND11 module for ad_params in Chimbuko Simulator"

  py::class_<ADalgParams>(m, "ADalgParams")
    .def(py::init<const std::string &, double, bool, double, ADOutlier::OutlierStatistic)
}
