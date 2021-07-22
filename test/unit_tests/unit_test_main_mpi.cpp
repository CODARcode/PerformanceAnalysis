#include "gtest/gtest.h"
#include<cassert>
#include<mpi.h>
#include "chimbuko/verbose.hpp"
#include "unit_test_cmdline.hpp"

int _argc;
char** _argv;

int main(int argc, char **argv) {
  assert( MPI_Init(&argc, &argv) == MPI_SUCCESS );

  chimbuko::enableVerboseLogging() = true;
  ::testing::InitGoogleTest(&argc, argv);

  _argc = argc;
  _argv = argv;

  int ret = RUN_ALL_TESTS();
  
  assert(MPI_Finalize() == MPI_SUCCESS );
  return ret;
}
