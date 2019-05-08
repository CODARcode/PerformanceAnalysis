#include <iostream>
#include <mpi.h>
#include <string.h>

int main (int argc, char** argv)
{
    MPI_Comm server;
    char port_name[MPI_MAX_PORT_NAME];
    int i, tag;

    // if (argc < 2) {
    //     std::cerr << "Server port name required!\n";
    //     exit(EXIT_FAILURE);
    // }

    MPI_Init(&argc, &argv);
    // strcpy(port_name, argv[1]);
    // std::cout << "Server at " << port_name << std::endl;
    std::cout << "Looking up server ...\n";
    MPI_Lookup_name("server", MPI_INFO_NULL, port_name);
    std::cout << "Connect to server: " << port_name << std::endl;
    MPI_Comm_connect(port_name, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &server);
    std::cout << "Start send ...\n";
    for (i = 0; i < 5; i++) {
        tag = 2;
        
        MPI_Send(&i, 1, MPI_INT, 0, tag, server);
        //MPI_Isend(buf, count, datatype, dest, tag, comm, &req);
    }
    std::cout << "Send disconnect request!\n";
    MPI_Send(&i, 0, MPI_INT, 0, 1, server);

    std::cout << "Disconnect server!\n";
    MPI_Comm_disconnect(&server);
    std::cout << "Finalize!\n";
    MPI_Finalize();
    return EXIT_SUCCESS;
}