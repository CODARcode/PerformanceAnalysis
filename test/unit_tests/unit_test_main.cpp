#include "gtest/gtest.h"
#include "chimbuko/verbose.hpp"
#include "unit_test_cmdline.hpp"

int _argc;
char** _argv;

int main(int argc, char **argv) {
  chimbuko::enableVerboseLogging() = true;
  ::testing::InitGoogleTest(&argc, argv);
  _argc = argc;
  _argv = argv;
  return RUN_ALL_TESTS();
}
