#include <chimbuko_config.h>
#include <gtest/gtest.h>
#ifdef USE_MPI
#include <mpi.h>
#endif
#include <chimbuko/verbose.hpp>

int main(int argc, char** argv)
{
  //chimbuko::Verbose::set_verbose(true);
  int result = 0;
  int provided;
  ::testing::InitGoogleTest(&argc, argv);

  // for (int i = 1; i < argc; i++)
  //     printf("arg %2d = %s\n", i, argv[i]);
#ifdef USE_MPI
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
#endif
  result = RUN_ALL_TESTS();
#ifdef USE_MPI
  MPI_Finalize();
#endif
  return result;
}
