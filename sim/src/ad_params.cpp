#include<sim/ad_params.hpp>
#include<random>

using namespace chimbuko;
using namespace chimbuko_sim;

void chimbuko_sim::registerFunc(const std::string &func_name, unsigned long mean_runtime, unsigned long std_dev_runtime, int seen_count){ 
  static unsigned long fidx_s = 0;
  unsigned long fidx = fidx_s++;
  
  funcIdxMap()[func_name] = fidx;
  RunStats &stats = globalParams()[fidx];

  static std::default_random_engine gen(1234);
  std::normal_distribution<double> dist(mean_runtime,std_dev_runtime);
  
  for(int i=0;i<seen_count;i++){
    unsigned long val = dist(gen);
    stats.push(val);
  }
  std::cout << "RunStats for func " << func_name << " : " << stats.get_json().dump() << std::endl;
}
