#include<sim/id_map.hpp>

using namespace chimbuko_sim;

std::unordered_map<std::string, unsigned long> & chimbuko_sim::funcIdxMap(){ static std::unordered_map<std::string, unsigned long> _map; return _map; }

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
