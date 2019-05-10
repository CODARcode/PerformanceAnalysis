#include "chimbuko/param/sstd_param.hpp"
#include <iostream>
#include <mpi.h>
#include <string.h>

int main (int argc, char** argv)
{
    MPI_Comm server;
    char port_name[MPI_MAX_PORT_NAME];
    int size, rank;
    int i, tag;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    MPI_Lookup_name("MPINET", MPI_INFO_NULL, port_name);
    /*
     * MPI_Comm_connect with MPI_COMM_WORLD
     * Even if all ranks call the MPI_Comm_connect(), mpi will remember the connection
     * after the first call, and as the result
     * - on client side: only the first call will actually attempt to get connection
     *                   and the other calls will copy the existing connection.
     * - on server side: there will be only one execution of MPI_Comm_accept().
     */
    //MPI_Comm_connect(port_name, MPI_INFO_NULL, 0, MPI_COMM_SELF, &server);
    MPI_Comm_connect(port_name, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &server);
    MPI_Barrier(MPI_COMM_WORLD);

    std::cout << "[" << rank << "]" << "Start send ...\n";
    for (i = 0; i < 5; i++) {
        tag = 2;
        int msg = i + 1000*rank;        
        MPI_Send(&msg, 1, MPI_INT, 0, tag, server);
    }
    //std::cout << "Send disconnect request!\n";

    // std::cout << "Disconnect server!\n";
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0)
        MPI_Send(&i, 0, MPI_INT, 0, 1, server);

    MPI_Comm_disconnect(&server);
    // std::cout << "Finalize!\n";
    
    MPI_Finalize();
    return EXIT_SUCCESS;

    // MPI_Comm server;
    // char port_name[MPI_MAX_PORT_NAME];
    // int i, tag;

    // MPI_Init(&argc, &argv);
    // std::cout << "Looking up server ...\n";
    // MPI_Lookup_name("server", MPI_INFO_NULL, port_name);
    // std::cout << "Connect to server: " << port_name << std::endl;
    // MPI_Comm_connect(port_name, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &server);
    // std::cout << "Start send ...\n";
    // for (i = 0; i < 5; i++) {
    //     tag = 2;
        
    //     MPI_Send(&i, 1, MPI_INT, 0, tag, server);
    // }
    // std::cout << "Send disconnect request!\n";
    // MPI_Send(&i, 0, MPI_INT, 0, 1, server);

    // std::cout << "Disconnect server!\n";
    // MPI_Comm_disconnect(&server);
    // std::cout << "Finalize!\n";
    // MPI_Finalize();
    // return EXIT_SUCCESS;
}