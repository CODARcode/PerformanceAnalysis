#include<chimbuko/modules/performance_analysis/pserver/PSfunctions.hpp>
#include<chimbuko/core/util/error.hpp>
#include<chimbuko/verbose.hpp>
#include<fstream>

using namespace chimbuko;

void chimbuko::writeModel(const std::string &filename, const PSglobalFunctionIndexMap &global_func_index_map, const PSparamManager &param){
  std::ofstream out(filename);
  if(!out.good()) fatal_error("Could not write anomaly algorithm parameters to the file provided");
  nlohmann::json out_p;
  out_p["func_index_map"] = global_func_index_map.serialize();
  out_p["alg_params"] = param.getGlobalModelJSON();
  out << out_p;
}

void chimbuko::restoreModel(PSglobalFunctionIndexMap &global_func_index_map,  PSparamManager &param, const std::string &filename){
  std::ifstream in(filename);
  if(!in.good()) fatal_error("Could not load anomaly algorithm parameters from the file provided");
  nlohmann::json in_p;
  in >> in_p;
  global_func_index_map.deserialize(in_p["func_index_map"]);
  param.restoreGlobalModelJSON(in_p["alg_params"]);
}
