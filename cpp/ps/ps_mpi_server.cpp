#include <iostream>
#include <mpi.h>

int main (int argc, char ** argv)
{
    MPI_Comm client;
    MPI_Status status;
    MPI_Info info;
    char port_name[MPI_MAX_PORT_NAME];
    int size, again, i;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    if (size != 1) {
        std::cerr << "Server MPI processors must be 1\n";
        exit(1);
    }

    MPI_Open_port(MPI_INFO_NULL, port_name);
    std::cout << "Server available at: " << port_name << std::endl;

    MPI_Info_create(&info);
    MPI_Info_set(info, "ompi_global_scope", "true");
    MPI_Publish_name("server", info, port_name);
    
    //printf("Server at: %s\n", port_name);
    while (true) {
        std::cout << "Waiting for connection ....\n";
        MPI_Comm_accept(port_name, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &client);

        again = 1;
        while (again) {
            MPI_Recv(&i, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, client, &status);
            switch (status.MPI_TAG)
            {
            case 0:
                std::cout << "Shut down server!\n";
                MPI_Unpublish_name("ocean", MPI_INFO_NULL, port_name);
                MPI_Comm_free(&client);
                MPI_Close_port(port_name);
                MPI_Finalize();
                return 0;
            
            case 1:
                std::cout << "Disconnect client!\n";
                MPI_Comm_disconnect(&client);
                std::cout << "check!\n";
                again = 0;
                break;

            case 2:
                std::cout << "Received: " << i << std::endl;
                break;

            default:
                std::cout << "Abort with unknown tag!\n";
                MPI_Abort(MPI_COMM_WORLD, 1);
                break;
            } // switch
        } // communication loop
    } // infinite loop

    MPI_Finalize();
    return 0;
}
