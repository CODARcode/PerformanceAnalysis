#pragma once
#include<chimbuko_config.h>
#include<chimbuko/core/chimbuko.hpp>
#include<chimbuko/core/util/commandLineParser.hpp>

namespace chimbuko{
  /**
   * @brief Insert commandline parser hooks for parsing base params
   */
  void setupBaseOptionalArgs(commandLineParser &parser, ChimbukoBaseParams &into);
};


