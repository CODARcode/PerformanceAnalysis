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
    int nt = -1, n_ad_modules = 0;
    std::string outFile = "./parameters.txt";
    std::string ws_addr;
#ifdef _USE_MPINET
    int provided;
    chimbuko::MPINet net;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
#else
    chimbuko::ZMQNet net;
    MPI_Init(&argc, &argv);
#endif

    try {
        if (argc > 1) {
            nt = atoi(argv[1]);
        }
        if (argc > 2) {
            outFile = std::string(argv[2]);
        }
        if (argc > 3) {
            n_ad_modules = atoi(argv[3]);
            ws_addr = std::string(argv[4]);
        }

        if (nt <= 0) {
            nt = std::max(
                (int)std::thread::hardware_concurrency() - 5,
                1
            );
        }

        //nt = std::max((int)std::thread::hardware_concurrency() - 2, 1);
        std::cout << "Run parameter server with " << nt << " threads" << std::endl;
        if (ws_addr.size() && n_ad_modules)
        {
            std::cout << "Run anomaly statistics sender (ws @ " << ws_addr << ")." << std::endl;
            param.reset_anomaly_stat({n_ad_modules});
        }

        // Note: for some reasons, internal MPI initialization cause segmentation error!! 
        net.set_parameter( dynamic_cast<chimbuko::ParamInterface*>(&param) );
        net.init(nullptr, nullptr, nt);
        net.run_stat_sender(ws_addr, false);
        net.run();

        // at this point, all pseudo AD modules finished sending 
        // anomaly statistics data
        std::this_thread::sleep_for(std::chrono::seconds(1));
        net.stop_stat_sender(1000);

        // could be output to a file
        std::cout << "Shutdonw parameter server ..." << std::endl;
        //param.show(std::cout);
        std::ofstream o;
        o.open(outFile);
        if (o.is_open())
        {
            param.show(o);
            o.close();
        }
    }
    catch (std::invalid_argument &e)
    {
        std::cout << e.what() << std::endl;
        //todo: usages()
    }
    catch (std::ios_base::failure &e)
    {
        std::cout << "I/O base exception caught\n";
        std::cout << e.what() << std::endl;
    }
    catch (std::exception &e)
    {
        std::cout << "Exception caught\n";
        std::cout << e.what() << std::endl;
    }

    net.finalize();
#ifdef _USE_ZMQNET
    MPI_Finalize();
#endif
    return 0;
}