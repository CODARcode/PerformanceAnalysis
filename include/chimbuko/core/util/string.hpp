#pragma once
#include <chimbuko_config.h>
#include <sstream>
#include <vector>
#include <cstdarg>
namespace chimbuko{

/**
   @brief Convert string to anything
*/
template<typename T>
T strToAny(const std::string &s){
  T out;
  std::istringstream ss(s); ss >> out;
  if(ss.fail()) throw std::runtime_error("Failed to parse \"" + s + "\"");
  return out;
}
template<>
inline std::string strToAny<std::string>(const std::string &s){ return s; }

template<>
inline bool strToAny<bool>(const std::string &s){
  bool out;
  std::istringstream ss(s); ss >> std::boolalpha >> out;
  if(ss.fail()){
    std::istringstream ss2(s); ss2 >> out;
    if(ss2.fail()) throw std::runtime_error("Failed to parse \"" + s + "\"");
  }
  return out;
}

/**
 * @brief Convert any type to string
 */
template<typename T>
std::string anyToStr(const T &v){
  std::stringstream ss; ss << v;
  return ss.str();
}
template<>
inline std::string anyToStr<std::string>(const std::string &s){ return s; }

template<>
inline std::string anyToStr<bool>(const bool &v){ 
  std::stringstream ss; ss << std::boolalpha << v;
  return ss.str();
}

  
/**
 * @brief C-style string formatting but without the nasty mem buffer concerns
 */
std::string stringize(const char* format, ...);

/**
 * @brief Break up a string into an array of strings around some delimiter
 */
std::vector<std::string> parseStringArray(const std::string &array, char delimiter);

};


