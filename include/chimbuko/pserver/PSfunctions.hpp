#pragma once

#include<chimbuko_config.h>
#include <chimbuko/pserver/PSglobalFunctionIndexMap.hpp>
#include <chimbuko/core/pserver/PSparamManager.hpp>

namespace chimbuko{
  
  /**
   * @brief Write the algorithm model and the mapping between global function index and name to a file
   */
  void writeModel(const std::string &filename, const PSglobalFunctionIndexMap &global_func_index_map, const PSparamManager &param);

  /**
   * @brief Restore the algorithm model and the mapping between global function index and name from a file
   *
   * WARNING: Overwrites any existing data in the function index map. As such this should only be used before clients start requesting these maps
   */
  void restoreModel(PSglobalFunctionIndexMap &global_func_index_map,  PSparamManager &param, const std::string &filename);

}
