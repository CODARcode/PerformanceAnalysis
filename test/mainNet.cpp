#include <gtest/gtest.h>
#include <mpi.h>
#include <chimbuko/verbose.hpp>

int main(int argc, char** argv)
{
  chimbuko::Verbose::set_verbose(true);
  int result = 0;
  int provided;
  ::testing::InitGoogleTest(&argc, argv);

  // for (int i = 1; i < argc; i++)
  //     printf("arg %2d = %s\n", i, argv[i]);

  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  result = RUN_ALL_TESTS();
  MPI_Finalize();
  return result;
}
