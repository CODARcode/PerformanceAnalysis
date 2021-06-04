#include<sim/id_map.hpp>
#include<chimbuko/util/error.hpp>

using namespace chimbuko_sim;

std::unordered_map<std::string, unsigned long> & chimbuko_sim::funcIdxMap(){ static std::unordered_map<std::string, unsigned long> _map; return _map; }

unsigned long chimbuko_sim::registerFunc(const std::string &func_name){ 
  if(funcIdxMap().count(func_name) != 0) fatal_error("Attempting to register function " + func_name + " more than once");

  static unsigned long fidx_s = 0; 
  unsigned long fidx = fidx_s++;
  funcIdxMap()[func_name] = fidx;
  return fidx;
}


long CounterIdxManager::getIndex(const std::string &cname){
  static int cid_cnt = 0;
  auto it = cid_map.find(cname);
  if(it == cid_map.end()){
    int cid = cid_cnt++;
    cid_map[cname] = cid;
    cname_map[cid] = cname;
    return cid;
  }else return it->second;
}
