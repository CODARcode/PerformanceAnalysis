#include <gtest/gtest.h>
#include <mpi.h>

int main(int argc, char** argv)
{
    int result = 0;
    ::testing::InitGoogleTest(&argc, argv);

    // for (int i = 1; i < argc; i++)
    //     printf("arg %2d = %s\n", i, argv[i]);

    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    ::testing::TestEventListeners& listeners = ::testing::UnitTest::GetInstance()->listeners();
    if (world_rank != 0)
        delete listeners.Release(listeners.default_result_printer());

    result = RUN_ALL_TESTS();
    // std::cout << world_rank << ": " << result << std::endl;
    
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return result;
}