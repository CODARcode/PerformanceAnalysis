#include <chimbuko/util/time.hpp>
#include <iomanip>
#include <ctime>
#include <sstream>

namespace chimbuko{

  std::string getDateTime(){
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y/%m/%d %H:%M:%S");
    return ss.str();
  }

  std::string getDateTimeFileExt(){
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y.%m.%d-%H.%M.%S");
    return ss.str();
  }


};
