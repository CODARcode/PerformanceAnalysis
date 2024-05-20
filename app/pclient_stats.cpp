//A test application that mocks part of the anomaly detection modules, acting as a client for the parameter server and sending it function statistics information
#include <chimbuko_config.h>

#include "chimbuko/core/param/sstd_param.hpp"
#include "chimbuko/core/message.hpp"
#include "chimbuko/modules/performance_analysis/ad/AnomalyData.hpp"
#include "chimbuko/modules/performance_analysis/ad/ADLocalFuncStatistics.hpp"
#include "chimbuko/modules/performance_analysis/pserver/PScommon.hpp"

#ifdef _USE_MPINET
#include "chimbuko/core/net/mpi_net.hpp"
#else
#include "chimbuko/core/net/zmq_net.hpp"
#endif

#ifdef USE_MPI
#include <mpi.h>
#endif

#include <iostream>
#include <string.h>
#include <random>

using namespace chimbuko;

int main (int argc, char** argv)
{
    const int N_MPI_PROCESSORS = 10;
    int size=1, rank=0;

#ifdef USE_MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (size > N_MPI_PROCESSORS)
    {
        std::cout << "Must -n 10" << std::endl;
        MPI_Finalize();
        return EXIT_SUCCESS;
    }
#endif

#ifdef _USE_MPINET
    throw std::runtime_error("Not implemented yet.");
#else
    void* context;
    void* socket;
    context = zmq_ctx_new();
    socket = zmq_socket(context, ZMQ_REQ);
    zmq_connect(socket, argv[1]);

    //Handshake
    {
      Message msg;
      msg.set_info(rank, 0, (int)MessageType::REQ_ECHO, (int)BuiltinMessageKind::DEFAULT);
      msg.setContent("");
      std::string strmsg;
      ZMQNet::send(socket, msg.serializeMessage());
      ZMQNet::recv(socket, strmsg);
    }
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

	ADLocalFuncStatistics fstat;
	fstat.setAnomalyData(d);

        // create message
        msg.clear();
        msg.set_info(rank, 0, MessageType::REQ_ADD, MessageKind::ANOMALY_STATS, step);
        msg.setContent(fstat.net_serialize());

#ifdef _USE_MPINET
	throw std::runtime_error("Not implemented yet.");
#else
        // send message to parameter server
        ZMQNet::send(socket, msg.serializeMessage());
        // std::cout << "Rank: " << rank << " sent " << step << "-th message!" << std::endl;

        // receive reply
        msg.clear();
        strmsg.clear();
        ZMQNet::recv(socket, strmsg);
        msg.deserializeMessage(strmsg);
        // std::cout << "Rank: " << rank << " receive " << step << "-th reply" << std::endl;
#endif

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif    

    // terminate parameter server
#ifdef _USE_MPINET
    throw std::runtime_error("Not implemented yet.");
#else
    msg.clear();
    msg.set_info(rank, 0, (int)MessageType::REQ_QUIT, (int)BuiltinMessageKind::DEFAULT);
    msg.setContent("");
    std::cout << "pclient_stats rank " << rank << " sending disconnect notification" << std::endl;
    ZMQNet::send(socket, msg.serializeMessage());
    std::cout << "pclient_stats rank " << rank << " waiting for disconnect notification response" << std::endl;
    ZMQNet::recv(socket, strmsg);
    std::cout << "pclient_stats rank " << rank << " exiting" << std::endl;
#endif


#ifdef _USE_MPINET
    throw "Not implemented yet."
#else
    zmq_close(socket);
    zmq_ctx_term(context);
#endif

#ifdef USE_MPI
    MPI_Finalize();
#endif
    return EXIT_SUCCESS;
}
