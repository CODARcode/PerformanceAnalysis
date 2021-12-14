#include <gtest/gtest.h>
#include<chimbuko_config.h>
#ifdef USE_MPI
#include <mpi.h>
#endif

int main(int argc, char** argv)
{
    int result = 0;
    ::testing::InitGoogleTest(&argc, argv);

    // for (int i = 1; i < argc; i++)
    //     printf("arg %2d = %s\n", i, argv[i]);
    int world_rank=0, world_size=1;
#ifdef USE_MPI
    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
#endif

    ::testing::TestEventListeners& listeners = ::testing::UnitTest::GetInstance()->listeners();
    if (world_rank != 0)
        delete listeners.Release(listeners.default_result_printer());

    result = RUN_ALL_TESTS();
    // std::cout << world_rank << ": " << result << std::endl;
    
#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
#endif
    return result;
}
