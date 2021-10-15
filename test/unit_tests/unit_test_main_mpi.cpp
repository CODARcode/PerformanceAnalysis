#include "gtest/gtest.h"
#include<cassert>
#include<chimbuko_config.h>
#ifdef USE_MPI
#include<mpi.h>
#endif
#include "chimbuko/verbose.hpp"
#include "unit_test_cmdline.hpp"

int _argc;
char** _argv;

int main(int argc, char **argv) {
#ifdef USE_MPI
  assert( MPI_Init(&argc, &argv) == MPI_SUCCESS );
#endif

  chimbuko::enableVerboseLogging() = true;
  ::testing::InitGoogleTest(&argc, argv);

  _argc = argc;
  _argv = argv;

  int ret = RUN_ALL_TESTS();
  
#ifdef USE_MPI
  assert(MPI_Finalize() == MPI_SUCCESS );
#endif
  return ret;
}
