#pragma once

#include <sstream>
#include <cstdarg>
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

/**
 * @brief Convert any type to string
 */
template<typename T>
std::string anyToStr(const T &v){
  std::string out;
  std::stringstream ss; ss << v; ss >> out;
  return out;
}
template<>
inline std::string anyToStr<std::string>(const std::string &s){ return s; }

  
/**
 * @brief C-style string formatting but without the nasty mem buffer concerns
 */
std::string stringize(const char* format, ...);

};


