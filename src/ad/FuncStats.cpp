#include<chimbuko/ad/FuncStats.hpp>

using namespace chimbuko;

FuncStats::State::State(const FuncStats &p):  pid(p.pid), id(p.id), name(p.name), n_anomaly(p.n_anomaly), 
					      inclusive(p.inclusive.get_state()), exclusive(p.exclusive.get_state()){}
  

nlohmann::json FuncStats::State::get_json() const{
  nlohmann::json obj;
  obj["pid"] = pid;
  obj["id"] = id;
  obj["name"] = name;
  obj["n_anomaly"] = n_anomaly;
  obj["inclusive"] = inclusive.get_json();
  obj["exclusive"] = exclusive.get_json();
  return obj;
}

void FuncStats::set_state(const State &to){
  pid = to.pid;
  id = to.id;
  name = to.name;
  n_anomaly = to.n_anomaly;
  inclusive.set_state(to.inclusive);
  exclusive.set_state(to.exclusive);
}

