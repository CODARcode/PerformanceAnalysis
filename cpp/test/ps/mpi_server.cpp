#ifdef _USE_MPINET
#include "chimbuko/net/mpi_net.hpp"
#include <mpi.h>
#endif

#include "chimbuko/param/sstd_param.hpp"

int main (int argc, char ** argv)
{
    chimbuko::MPINet net;
    chimbuko::SstdParam param;

    net.init(&argc, &argv, 10);
    net.set_parameter(&param);
    net.run();
    net.finalize();
    return 0;
}
