#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/message.hpp"

#ifdef _USE_MPINET
#include "chimbuko/net/mpi_net.hpp"
#else
#include "chimbuko/net/zmq_net.hpp"
#include <mpi.h>
#endif

#include <iostream>
#include <string.h>
#include <random>

using namespace chimbuko;

int main (int argc, char** argv)
{
    const int N_MPI_PROCESSORS = 10;
    int size, rank;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (size != N_MPI_PROCESSORS)
    {
        std::cout << "Must -n 10" << std::endl;
        MPI_Finalize();
        return EXIT_SUCCESS;
    }

#ifdef _USE_MPINET
    throw "Not implemented yet.";
#else
    void* context;
    void* socket;
    context = zmq_ctx_new();
    socket = zmq_socket(context, ZMQ_REQ);
    zmq_connect(socket, argv[1]);
#endif

    const std::vector<double> means = {
        100, 200, 300, 400, 500,
        150, 250, 350, 450, 550
    };
    const std::vector<double> stddevs = {
        10, 20, 30, 40, 50,
        15, 25, 35, 45, 55
    };
    const int MAX_STEPS = 1000;

    Message msg;
    std::string strmsg;
    std::normal_distribution<double> dist(means[rank], stddevs[rank]);
    std::default_random_engine generator;

    for (int step = 0; step < MAX_STEPS; step++)
    {
        int n_anomalies = std::max(0, (int)dist(generator));
        AnomalyData d(0, rank, step, 0, 0, n_anomalies);

        // create message
        msg.clear();
        msg.set_info(rank, 0, MessageType::REQ_ADD, MessageKind::ANOMALY_STATS, step);
        msg.set_msg(d.get_binary(), false);

#ifdef _USE_MPINET
    throw "Not implemented yet."
#else
        // send message to parameter server
        ZMQNet::send(socket, msg.data());
        // std::cout << "Rank: " << rank << " sent " << step << "-th message!" << std::endl;

        // receive reply
        msg.clear();
        strmsg.clear();
        ZMQNet::recv(socket, strmsg);
        msg.set_msg(strmsg, true);
        // std::cout << "Rank: " << rank << " receive " << step << "-th reply" << std::endl;
#endif
    }

    MPI_Barrier(MPI_COMM_WORLD);
    
    // terminate parameter server
    if (rank == 0) {
#ifdef _USE_MPINET
        throw "Not implemented yet."
#else
        zmq_send(socket, nullptr, 0, 0);
#endif
    }

#ifdef _USE_MPINET
    thow "Not implemented yet."
#else
    zmq_close(socket);
    zmq_ctx_term(context);
#endif

    MPI_Finalize();
    return EXIT_SUCCESS;
}