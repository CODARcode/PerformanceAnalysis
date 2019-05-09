#include "chimbuko/net.hpp"
#include <mpi.h>
#include <iostream>

int main(int argc, char** argv)
{
    using namespace chimbuko;

    NetInterface& net = DefaultNetInterface::get();
    std::cout << net.name() << std::endl;
    
    // int provided;
    // MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    // std::cout << "provided: " << provided << std::endl;
    // MPI_Finalize();

    net.init(&argc, &argv, 10);
    net.run();
    net.finalize();

    return 0;
}