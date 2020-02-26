#include "gtest/gtest.h"
#include<cassert>
#include<mpi.h>

int main(int argc, char **argv) {
  assert( MPI_Init(&argc, &argv) == MPI_SUCCESS );
  
  ::testing::InitGoogleTest(&argc, argv);
  int ret = RUN_ALL_TESTS();
  
  assert(MPI_Finalize() == MPI_SUCCESS );
}
