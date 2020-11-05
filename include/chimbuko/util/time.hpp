#pragma once
#include <sstream>
#include <iomanip>
#include <ctime>

namespace chimbuko{

  /**
   * @brief Get the local date and time
   */
  std::string getDateTime(){
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::stringstream ss;
    ss << std::put_time(&tm, "%m-%d-%Y %H-%M-%S");
    return ss.str();
  }




};
