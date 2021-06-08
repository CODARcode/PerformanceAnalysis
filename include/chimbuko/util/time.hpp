#pragma once
#include <string>

namespace chimbuko{

  /**
   * @brief Get the local date and time in format "YYYY/MM/DD HH:MM:SS"
   */
  std::string getDateTime();

  /**
   * @brief Get the local date and time in format YYYY.MM.DD-HH.MM.SS suitable for a filename extension
   */
  std::string getDateTimeFileExt();

};
