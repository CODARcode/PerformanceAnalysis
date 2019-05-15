#ifdef _USE_MPINET
#include "chimbuko/net/mpi_net.hpp"
#include <mpi.h>
#endif

#include "chimbuko/param/sstd_param.hpp"

int main (int argc, char ** argv)
{
    int provided;
    chimbuko::MPINet net;
    chimbuko::SstdParam param;

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

    // Note: for some reasons, internal MPI initialization cause segmentation error!! 
    net.init(nullptr, nullptr, 10);
    net.set_parameter(&param);
    net.run();

    MPI_Finalize();
    return 0;
}
