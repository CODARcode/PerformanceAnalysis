#ifdef _USE_MPINET
#include "chimbuko/net/mpi_net.hpp"
#include <mpi.h>
#else
#include "chimbuko/net/zmq_net.hpp"
#endif

#include "chimbuko/param/sstd_param.hpp"

int main (int argc, char ** argv)
{
    chimbuko::SstdParam param;
    int nt = 10;
#ifdef _USE_MPINET
    int provided;
    chimbuko::MPINet net;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
#else
    chimbuko::ZMQNet net;
#endif

    //nt = std::max((int)std::thread::hardware_concurrency() - 2, 1);
    std::cout << "Run parameter server with " << nt << " threads" << std::endl;

    // Note: for some reasons, internal MPI initialization cause segmentation error!! 
    net.init(nullptr, nullptr, nt);
    net.set_parameter(&param);
    net.run();

    param.show(std::cout);

#ifdef _USE_MPINET
    MPI_Finalize();
#endif
    return 0;
}
