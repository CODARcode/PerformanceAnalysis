#include "gtest/gtest.h"
#include "chimbuko/verbose.hpp"

int main(int argc, char **argv) {
  chimbuko::enableVerboseLogging() = true;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
