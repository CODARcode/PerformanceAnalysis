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
inline std::string stringize(const char* format, ...){
  int n; //not counting null character
  {
    char buf[1024];
    va_list argptr;
    va_start(argptr, format);    
    n = vsnprintf(buf, 1024, format, argptr);
    va_end(argptr);
    if(n < 1024) return std::string(buf);
  }
  
  char buf[n+1];
  va_list argptr;
  va_start(argptr, format);    
  int n2 = vsnprintf(buf, 1024, format, argptr);
  va_end(argptr);
  assert(n2 <= n);
  return std::string(buf);
}


};


