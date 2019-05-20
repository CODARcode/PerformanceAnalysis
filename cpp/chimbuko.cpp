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

    // -----------------------------------------------------------------------
    // parser command line arguments
    std::string engineType = argv[1]; // BPFile or SST
    std::string data_dir = argv[2]; // *.bp location
    std::string inputFile = "tau-metrics-" + std::to_string(world_rank) + ".bp";
    std::string output_dir = argv[3]; //output directory
#ifdef _USE_ZMQNET
    std::string addr = argv[4]; // (e.g. "tcp://hostname:5559")
#endif

    std::cout << "\n" 
              << "rank       : " << world_rank << "\n"
              << "Engine     : " << engineType << "\n"
              << "BP in dir  : " << data_dir << "\n"
              << "BP file    : " << inputFile << "\n"
              << "BP out dir : " << output_dir 
#ifdef _USE_ZMQNET
              << "\nPS Addr    : " << addr
#endif
              << std::endl;

    double sigma = 6.0;

    // -----------------------------------------------------------------------
    // AD module variables
    ADParser * parser;
    ADEvent * event;
    ADOutlierSSTD * outlier;
    ADio * io;

    int step = 0; 
    size_t idx_funcData = 0, idx_commData = 0, i = 0;
    const unsigned long *funcData = nullptr, *commData = nullptr;
    PQUEUE pq;

    // -----------------------------------------------------------------------
    // Measurement variables
    unsigned long total_frames = 0, frames = 0;
    unsigned long total_n_outliers = 0, n_outliers = 0;
    unsigned long total_processing_time = 0, processing_time = 0;
    high_resolution_clock::time_point t1, t2;


    // -----------------------------------------------------------------------
    // Init. AD module
    parser = new ADParser(data_dir + "/" + inputFile, engineType);
    event = new ADEvent();
    outlier = new ADOutlierSSTD();
    io = new ADio();

    event->linkFuncMap(parser->getFuncMap());
    event->linkEventType(parser->getEventType());

    outlier->linkExecDataMap(event->getExecDataMap());
    outlier->set_sigma(sigma);
#ifdef _USE_MPINET
    outlier->connect_ps(world_rank);
#else
    outlier->connect_ps(world_rank, 0, addr);
#endif

    io->setDispatcher();
    io->setHeader({{"rank", world_rank}, {"algorithm", 0}, {"nparam", 1}, {"winsz", 0}});

    // io->open_curl(); // for VIS module
    io->open(output_dir + "/execdata." + std::to_string(world_rank), IOOpenMode::Write); // for file output


    // -----------------------------------------------------------------------
    // Start analysis
    std::cout << "rank: " << world_rank << " analysis start " << (outlier->use_ps() ? "with": "without") << " pserver" << std::endl;
    t1 = high_resolution_clock::now();
    while ( parser->getStatus() )
    {
        parser->beginStep();
        if (!parser->getStatus())
            break;

        step = parser->getCurrentStep();
        parser->update_attributes();
        parser->fetchFuncData();
        parser->fetchCommData();

        idx_funcData = idx_commData = 0;
        funcData = nullptr;
        commData = nullptr;
        if (!pq.empty()) {
            std::cerr << "\n***** None empty priority queue!*****\n";
            exit(EXIT_FAILURE);
        }        

        // warm-up: currently, this is required to resolve the issue that `pthread_create` apprears 
        // at the beginning of the first frame even though it timestamp say it shouldn't be at the beginning.
        for (i = 0; i < 10; i++) {
            if ( (funcData = parser->getFuncData(idx_funcData)) != nullptr ) {
                pq.push(Event_t(
                    funcData, EventDataType::FUNC, idx_funcData, 
                    generate_event_id(world_rank, step, idx_funcData))
                );
                idx_funcData++;
            }
            if ( (commData = parser->getCommData(idx_commData)) != nullptr ) {
                pq.push(Event_t(commData, EventDataType::COMM, idx_commData));
                idx_commData++;
            }
        }

        while (!pq.empty()) {
            Event_t ev = pq.top();
            pq.pop();

            if (event->addEvent(ev) == EventError::CallStackViolation)
            {
                std::cerr << "\n***** Call stack violation *****\n";
                exit(EXIT_FAILURE);
            }

            switch (ev.type())
            {
            case EventDataType::FUNC:
                if ( (funcData = parser->getFuncData(idx_funcData)) != nullptr ) {
                    pq.push(Event_t(
                        funcData, EventDataType::FUNC, idx_funcData, 
                        generate_event_id(world_rank, step, idx_funcData))
                    );
                    idx_funcData++;
                }
                break;
            case EventDataType::COMM:
                // event->addFunc(ev, generate_event_id(world_rank, step))
                if ( (commData = parser->getCommData(idx_commData)) != nullptr ) {
                    pq.push(Event_t(commData, EventDataType::COMM, idx_commData));
                    idx_commData++;
                }
                break;
            default:
                break;
            }
        }        

        //outlier->run();
        n_outliers += outlier->run();
        frames++;
        //std::cout << n_outliers << std::endl;

        parser->endStep();

        io->write(event->trimCallList(), step);
    }
    t2 = high_resolution_clock::now();
    std::cout << "rank: " << world_rank << " analysis done!\n";

    // -----------------------------------------------------------------------
    // Average analysis time and total number of outliers
    MPI_Barrier(MPI_COMM_WORLD);
    processing_time = duration_cast<milliseconds>(t2 - t1).count();

    const unsigned long local_measures[] = {processing_time, n_outliers, frames};
    unsigned long global_measures[] = {0, 0, 0};
    MPI_Reduce(
        local_measures, global_measures, 3, MPI_UNSIGNED_LONG,
        MPI_SUM, 0, MPI_COMM_WORLD
    );
    // MPI_Reduce(&processing_time, &total_processing_time, 1, MPI_UNSIGNED_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    // MPI_Reduce(&n_outliers, &total_n_outliers, 1, MPI_UNSIGNED_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    // MPI_Reduce(&total_frames, &frames, 1, MPI_UNSIGNED_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    if (world_rank == 0) {
        total_processing_time = global_measures[0];
        total_n_outliers = global_measures[1];
        total_frames = global_measures[2];

        std::cout << "\n"
            << "Avg. num. frames     : " << (double)total_frames/(double)world_size << "\n"
            << "Avg. processing time : " << (double)total_processing_time/(double)world_size << " msec\n"
            << "Total num. outliers  : " << total_n_outliers 
            << std::endl;
    }

    // -----------------------------------------------------------------------
    // Finalize
    outlier->disconnect_ps();

    delete parser;
    delete event;
    delete outlier;
    delete io;

    MPI_Finalize();
    return 0;
}