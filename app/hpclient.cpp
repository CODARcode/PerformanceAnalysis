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
#define NO_OF_SERVERS 4

using namespace std;
using namespace chimbuko;

int main (int argc, char** argv)
{
    int size, rank;
    int group;
	
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
/*
 * The following  take care of the h_ parameter server
 *
 *
 */

    group = rank%(NO_OF_SERVERS);	
    char temp[40];
    sprintf(temp,"%d", group);
    char hostname[40]="wps.txt";
    char portnumber[40]="wport.txt";
	
    strcat(hostname,temp);
    strcat(portnumber,temp);

    char bufn[100];
    char bufp[100];
    FILE* filepointer;
    filepointer = fopen(hostname, "r");
    fgets( bufn, 100, filepointer ); 
    fclose(filepointer);
    filepointer = fopen(portnumber, "r");
    fgets( bufp, 100, filepointer);
    fclose(filepointer);
    sprintf(temp, "tcp://%s:%s",bufn,bufp);
  
    printf(" Taking to worker server at %s\n", temp); 	
    printf("%s\n", temp);

#ifdef _USE_MPINET
    int count;
    MPI_Comm server;
    MPI_Status status;
    char port_name[MPI_MAX_PORT_NAME];
    MPI_Lookup_name("MPINET", MPI_INFO_NULL, port_name);
    //strcpy(port_name, argv[1]);
    //std::cout << "[CLIENT] port name: " << port_name << std::endl;
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
#else
    void* context;
    void* socket;
    context = zmq_ctx_new();
    socket = zmq_socket(context, ZMQ_REQ);
    //zmq_connect(socket, "tcp://localhost:5559");
    //zmq_connect(socket, argv[1]);
    zmq_connect(socket, temp);

#endif
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
        msg.set_info(rank, 0, MessageType::REQ_ADD, MessageKind::SSTD, iFrame);
        msg.set_msg(l_param.serialize(), false);

#ifdef _USE_MPINET
        // send to server
        MPINet::send(server, msg.data(), 0, MessageType::REQ_ADD, msg.count());
        
    	// receive reply
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, server, &status);
        //MPI_Probe(0, MessageType::REP_ADD, server, &status);
        std::cout << "receive reply from server\n";
    	MPI_Get_count(&status, MPI_BYTE, &count);
        msg.clear();
        msg.set_msg(
            MPINet::recv(server, status.MPI_SOURCE, status.MPI_TAG, count),
            true
        );
#else
	cout << "[Main Client]Sending the message to the server" << endl;
        ZMQNet::send(socket, msg.data());

        msg.clear();
        std::string strmsg;
        ZMQNet::recv(socket, strmsg);
	cout << "[Main Client] receiveing the message from the server" << endl;
        msg.set_msg(strmsg, true);
#endif
        //g_param.assign(msg.data_buffer());
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
#ifdef _USE_MPINET
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
#else
        zmq_send(socket, nullptr, 0, 0);
#endif
    }

#ifdef _USE_MPINET
    MPI_Comm_disconnect(&server);
#else
    zmq_close(socket);
    zmq_ctx_term(context);
#endif

    MPI_Finalize();
    return EXIT_SUCCESS;
}
