#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/message.hpp"
#include "chimbuko/net/mpi_net.hpp"
#include <iostream>
#include <mpi.h>
#include <string.h>
#include <random>

using namespace chimbuko;

int main (int argc, char** argv)
{
    MPI_Comm server;
    MPI_Status status;
    char port_name[MPI_MAX_PORT_NAME];
    int size, rank, count;

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

    const int n_functions = 4;
    const double mean[n_functions] = {1.0, 10.0, 100.0, 1000.0};
    const double std[n_functions] = {0.1, 1.0, 10.0, 100.0};
    const int n_rolls = 1000;
    const int n_frames = 100;
    SstdParam l_param, g_param, c_param;
    Message msg;
    std::default_random_engine generator;
    std::unordered_map<unsigned long, std::vector<double>> data;

    for (int iFrame = 0; iFrame < n_frames; iFrame++) {
        l_param.clear();
        for (int i = 0; i < n_functions; i++)
        {
            std::normal_distribution<double> dist(mean[i], std[i]);
            unsigned long funcid = (unsigned long)i;
            for (int j = 0; j < n_rolls; j++) 
            {
                double num = dist(generator);
                l_param[funcid].push(num);
                data[funcid].push_back(num);
            }
        }
        // set message
        msg.clear();
        msg.set_info(rank, 0, (int)MessageType::REQ_ADD, (int)MessageKind::SSTD, iFrame);
        msg.set_msg(l_param.serialize(), false);

        // send to server
        MPINet::send(server, msg.data(), 0, MessageType::REQ_ADD, msg.count());
        // receive reply
        //MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, server, &status);
        MPI_Probe(0, MessageType::REP_ADD, server, &status);
        MPI_Get_count(&status, MPI_BYTE, &count);
        msg.clear();
        msg.set_msg(
            MPINet::recv(server, status.MPI_SOURCE, status.MPI_TAG, count),
            true
        );
        //std::cout << status.MPI_SOURCE << std::endl;

        g_param.assign(msg.data_buffer());
    }

    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        msg.clear();
        msg.set_info(rank, 0, (int)MessageType::REQ_GET, MessageKind::SSTD);
        MPINet::send(server, msg.data(), 0, MessageType::REQ_GET, msg.count());

        // receive reply
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, server, &status);
        MPI_Get_count(&status, MPI_BYTE, &count);
        msg.clear();
        msg.set_msg(
            MPINet::recv(server, status.MPI_SOURCE, status.MPI_TAG, count),
            true
        );

        c_param.update(msg.data_buffer(), false);

        msg.clear();
        msg.set_info(rank, 0, (int)MessageType::REQ_QUIT, (int)MessageKind::DEFAULT);
        msg.set_msg(MessageCmd::QUIT);
        MPINet::send(server, msg.data(), 0, MessageType::REQ_QUIT, msg.count());
    }
    MPI_Comm_disconnect(&server);
    MPI_Finalize();
    return EXIT_SUCCESS;
}