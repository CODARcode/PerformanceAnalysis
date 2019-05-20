#ifdef _USE_MPINET
#include "chimbuko/net/mpi_net.hpp"
#else
#include "chimbuko/net/zmq_net.hpp"
#endif
#include <mpi.h>
#include "chimbuko/param/sstd_param.hpp"
#include <fstream>

int main (int argc, char ** argv)
{
    chimbuko::SstdParam param;
    int nt = -1;
    std::string outFile = "./parameters.txt";
#ifdef _USE_MPINET
    int provided;
    chimbuko::MPINet net;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
#else
    chimbuko::ZMQNet net;
    MPI_Init(&argc, &argv);
#endif

    if (argc > 1) {
        nt = atoi(argv[1]);
    }
    if (argc > 2) {
        outFile = std::string(argv[2]);
    }

    if (nt <= 0) {
        nt = std::max(
            (int)std::thread::hardware_concurrency() - 5,
            1
        );
    }

    //nt = std::max((int)std::thread::hardware_concurrency() - 2, 1);
    std::cout << "Run parameter server with " << nt << " threads" << std::endl;

    // Note: for some reasons, internal MPI initialization cause segmentation error!! 
    net.init(nullptr, nullptr, nt);
    net.set_parameter(&param);
    net.run();

    // could be output to a file
    //param.show(std::cout);
    std::ofstream o;
    o.open(outFile);
    if (o.is_open())
    {
        param.show(o);
        o.close();
    }

    net.finalize();
#ifdef _USE_ZMQNET
    MPI_Finalize();
#endif
    return 0;
}