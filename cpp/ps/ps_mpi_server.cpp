#include <iostream>
#include <mpi.h>
#include <thread>
#include <mutex>
#include <vector>

std::mutex mio;

void foo(MPI_Comm* client)
{
    {
        std::lock_guard _(mio);
        std::cout << "I am " << std::this_thread::get_id() << std::endl;
    }
    MPI_Status status;
    int i;

    while (client != nullptr) {
        // MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, *client, &status);
        // {
        //     std::lock_guard _(mio);
        //     std::cout << "Got " << status.MPI_TAG << " from " << status.MPI_SOURCE << std::endl;    
        // }

        MPI_Recv(&i, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, *client, &status);
        //MPI_Probe(source, tag, comm, status);
        switch (status.MPI_TAG)
        {
        case 0:
            break;
            // std::cout << "Shut down server!\n";
            // MPI_Unpublish_name("ocean", MPI_INFO_NULL, port_name);
            // MPI_Comm_free(&client);
            // MPI_Close_port(port_name);
            // MPI_Finalize();
            // return 0;
        
        case 1:
            {
                std::lock_guard _(mio);
                std::cout << "[" << std::this_thread::get_id() << "] ";
                std::cout << "Disconnect client!\n";
                MPI_Comm_disconnect(client);
                delete client;
                client = nullptr;
            }
            break;

        case 2:
            {
                std::lock_guard _(mio);
                std::cout << "[" << std::this_thread::get_id() << "] ";
                std::cout << "Received: " << i << " from " << status.MPI_SOURCE << std::endl;
            }
            break;

        default:
            break;
        } // switch
    } // communication loop    
}

int main (int argc, char ** argv)
{
    int provided;
    MPI_Info info;
    char port_name[MPI_MAX_PORT_NAME];    
    
    std::vector<std::thread> threads;

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    MPI_Open_port(MPI_INFO_NULL, port_name);
    {
        std::lock_guard _(mio);
        std::cout << "I am " << std::this_thread::get_id() << std::endl;
        std::cout << "Server available at: " << port_name << std::endl;
    }
    MPI_Info_create(&info);
    MPI_Info_set(info, "ompi_global_scope", "true");
    MPI_Publish_name("server", info, port_name);    

    int n_clients = 0;
    for (;;)
    {
        MPI_Comm * client = new MPI_Comm();
        MPI_Comm_accept(port_name, MPI_INFO_NULL, 0, MPI_COMM_WORLD, client);

        n_clients++;
        std::cout << "Establish connection with client " << n_clients << std::endl;

        threads.emplace_back(&foo, client);
    }


    // while (true) {
    //     std::cout << "Waiting for connection ....\n";
    //     MPI_Comm_accept(port_name, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &client);
    // } // infinite loop

    for (auto& t: threads) 
        if (t.joinable())
            t.join();


    MPI_Finalize();
    // MPI_Comm client;
    // MPI_Status status;
    // MPI_Info info;
    // char port_name[MPI_MAX_PORT_NAME];
    // int size, again, i;

    // MPI_Init(&argc, &argv);
    // MPI_Comm_size(MPI_COMM_WORLD, &size);
    // if (size != 1) {
    //     std::cerr << "Server MPI processors must be 1\n";
    //     exit(1);
    // }

    // MPI_Open_port(MPI_INFO_NULL, port_name);
    // std::cout << "Server available at: " << port_name << std::endl;

    // MPI_Info_create(&info);
    // MPI_Info_set(info, "ompi_global_scope", "true");
    // MPI_Publish_name("server", info, port_name);
    
    // //printf("Server at: %s\n", port_name);
    // while (true) {
    //     std::cout << "Waiting for connection ....\n";
    //     MPI_Comm_accept(port_name, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &client);

    //     again = 1;
    //     while (again) {
    //         MPI_Recv(&i, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, client, &status);
    //         switch (status.MPI_TAG)
    //         {
    //         case 0:
    //             std::cout << "Shut down server!\n";
    //             MPI_Unpublish_name("ocean", MPI_INFO_NULL, port_name);
    //             MPI_Comm_free(&client);
    //             MPI_Close_port(port_name);
    //             MPI_Finalize();
    //             return 0;
            
    //         case 1:
    //             std::cout << "Disconnect client!\n";
    //             MPI_Comm_disconnect(&client);
    //             std::cout << "check!\n";
    //             again = 0;
    //             break;

    //         case 2:
    //             std::cout << "Received: " << i << std::endl;
    //             break;

    //         default:
    //             std::cout << "Abort with unknown tag!\n";
    //             MPI_Abort(MPI_COMM_WORLD, 1);
    //             break;
    //         } // switch
    //     } // communication loop
    // } // infinite loop

    // MPI_Finalize();
    return 0;
}
