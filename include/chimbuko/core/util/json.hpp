#pragma once
#include <chimbuko_config.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace chimbuko {

  /**
   * @brief Return an element of a JSON document with a particular key. To accommodate nested data structures, each key is a vector of strings where each string maps to a sub-array key-index, eg {"data","metrics","value"} ->   j["data"]["metrics"]["value"]
   */
  nlohmann::json const* get_json_elem(const std::vector<std::string> &key, const nlohmann::json &j);

  /**
   * @brief Sort a JSON array according to a list of keys, with earlier keys in the list having priority
   * @param j The JSON array
   * @param by_keys The array of keys. To accommodate nested data structures, each key is a vector of strings where each string maps to a sub-array key-index, eg {"data","metrics","value"} ->   j["data"]["metrics"]["value"]
   */
  void sort_json(nlohmann::json &j, const std::vector<std::vector<std::string> > &by_keys);
}
