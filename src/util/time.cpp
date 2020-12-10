#include <chimbuko/util/time.hpp>
#include <iomanip>
#include <ctime>
#include <sstream>

namespace chimbuko{

  std::string getDateTime(){
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::stringstream ss;
    ss << std::put_time(&tm, "%m-%d-%Y %H-%M-%S");
    return ss.str();
  }

};
