#include<chimbuko/util/string.hpp>
#include<cassert>

namespace chimbuko{

std::string stringize(const char* format, ...){
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

}
