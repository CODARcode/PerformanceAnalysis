#pragma once

#include<sstream>

namespace chimbuko{

/**
   @brief Convert string to anything
*/
template<typename T>
T strToAny(const std::string &s){
  T out;
  std::stringstream ss; ss << s; ss >> out;
  if(ss.fail()) throw std::runtime_error("Failed to parse \"" + s + "\"");
  return out;
}
template<>
inline std::string strToAny<std::string>(const std::string &s){ return s; }

};
