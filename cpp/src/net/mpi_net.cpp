#ifdef CHIMBUKO_USE_MPINET
#include "chimbuko/net/mpi_net.hpp"

// todo: replace with log
#include <iostream> 

#include <mpi.h>
using namespace chimbuko;

MPINet::MPINet() : m_inited(0)
{
}

MPINet::~MPINet()
{
}

void MPINet::init(int* argc, char*** argv, int nt)
{    
    MPI_Initialized(&m_inited);
    if (!m_inited) 
    {
        MPI_Init_thread(argc, &(*argv), MPI_THREAD_MULTIPLE, &m_thread_provided);
    }

    MPI_Query_thread(&m_thread_provided);
    if (m_thread_provided != MPI_THREAD_MULTIPLE)
    {
        std::cerr << "Currenlty only support MPI_THREAD_MULTIPLE mode\n";
        exit(1);
    }

    MPI_Comm_size(MPI_COMM_WORLD, &m_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &m_rank);
    MPI_Barrier(MPI_COMM_WORLD);
    if (m_size > 1)
    {
        std::cerr << "Currently MPINET must be size of 1!\n";
        exit(1);
    }
    
    init_thread_pool(nt);
}

void MPINet::finalize()
{
    m_inited = 0;
    MPI_Finalize();
}

void MPINet::run()
{
    MPI_Info info;
    MPI_Comm client;
    MPI_Status status;
    char port_name[MPI_MAX_PORT_NAME];

    MPI_Open_port(MPI_INFO_NULL, port_name);
    MPI_Info_create(&info);
    MPI_Info_set(info, "ompi_global_scope", "true");
    MPI_Publish_name(name().c_str(), info, port_name);

    // single connection only!!
    MPI_Comm_accept(port_name, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &client);

    while (true)
    {
        std::cout << "Waiting ... \n";
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, client, &status);
        //status.MPI_SOURCE
        //status.MPI_TAG
        // m_tpool->sumit([&client, this](){
        //     MPI_Status _status;
        //     int i;
        //     MPI_Recv(&i, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, client, &_status);
        //     this->update(i);
        // });
    }
}


#endif // CHIMBUKO_USE_MPINET
