#pragma once
#include<chimbuko_config.h>
#include<string>

namespace chimbuko{

  /**
   * @brief Get the hostname of the syste,
   *
   * Hostname is determined the first time the function is called and is maintained as a static string
   */
  const std::string &getHostname();

};
