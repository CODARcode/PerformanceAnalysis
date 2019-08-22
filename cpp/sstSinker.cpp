#include "chimbuko/AD.hpp"
#include <chrono>

using namespace chimbuko;
using namespace std::chrono;

// input argument
// - engineType (for BP, + data_dir)
// - output_dir (for now) to dump

// soon later
// - inputFile prefix (i.e. tau-metrics)
// - sigma
// - ps server name 

int main(int argc, char ** argv)
{
    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);    

    try 
    {
        // -----------------------------------------------------------------------
        // parser command line arguments
        // -----------------------------------------------------------------------
        std::string engineType = argv[1]; // BPFile or SST
        std::string data_dir = argv[2]; // *.bp location
        std::string inputFile = "tau-metrics-" + std::to_string(world_rank) + ".bp";
        int fetch_data = atoi(argv[3]);

        if (world_rank == 0) {
        std::cout << "\n" 
                << "rank       : " << world_rank << "\n"
                << "Engine     : " << engineType << "\n"
                << "BP in dir  : " << data_dir << "\n"
                << "BP file    : " << inputFile << "\n"
                << "Fetch      : " << fetch_data
                << std::endl;
        }

        // -----------------------------------------------------------------------
        // AD module variables
        // -----------------------------------------------------------------------
        ADParser * parser;

        // int step = 0; 

        // -----------------------------------------------------------------------
        // Measurement variables
        // -----------------------------------------------------------------------
        unsigned long total_frames = 0, frames = 0;
        unsigned long total_processing_time = 0, processing_time = 0;
        high_resolution_clock::time_point t1, t2;

        // -----------------------------------------------------------------------
        // Init. AD module
        // First, init io to make sure file (or connection) handler
        // -----------------------------------------------------------------------
        parser = new ADParser(data_dir + "/" + inputFile, engineType);

        // -----------------------------------------------------------------------
        // Start analysis
        // -----------------------------------------------------------------------
        if (world_rank == 0) {
            std::cout << "rank: " << world_rank << std::endl;
        }
        t1 = high_resolution_clock::now();
        while ( parser->getStatus() )
        {
            parser->beginStep();
            if (!parser->getStatus())
            {
                // No more steps available.
                break;                
            }

            // step = parser->getCurrentStep();

            if (fetch_data) {
                parser->update_attributes();
                parser->fetchFuncData();
                parser->fetchCommData();
            }

            frames++;
            parser->endStep();
        }
        t2 = high_resolution_clock::now();
        if (world_rank == 0) {
            std::cout << "rank: " << world_rank << " analysis done!\n";
        }

        // -----------------------------------------------------------------------
        // Average analysis time and total number of outliers
        // -----------------------------------------------------------------------
        MPI_Barrier(MPI_COMM_WORLD);
        processing_time = duration_cast<milliseconds>(t2 - t1).count();

        {
            const unsigned long local_measures[] = {processing_time, frames};
            unsigned long global_measures[] = {0, 0};
            MPI_Reduce(
                local_measures, global_measures, 2, MPI_UNSIGNED_LONG,
                MPI_SUM, 0, MPI_COMM_WORLD
            );
            total_processing_time = global_measures[0];
            total_frames = global_measures[1];
        }
        
        if (world_rank == 0) {
            std::cout << "\n"
                << "Avg. num. frames     : " << (double)total_frames/(double)world_size << "\n"
                << "Avg. processing time : " << (double)total_processing_time/(double)world_size << " msec\n"
                << std::endl;
        }

        // -----------------------------------------------------------------------
        // Finalize
        // -----------------------------------------------------------------------
        delete parser;
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

    MPI_Finalize();
    return 0;
}
