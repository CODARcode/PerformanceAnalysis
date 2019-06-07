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
        std::string output_dir = argv[3]; //output directory
        std::string addr;
#ifdef _USE_ZMQNET
        if (argc == 5)
            addr = std::string(argv[4]); // (e.g. "tcp://hostname:5559")
#endif

        if (world_rank == 0) {
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
        }

        double sigma = 6.0;

        // -----------------------------------------------------------------------
        // AD module variables
        // -----------------------------------------------------------------------
        ADParser * parser;
        ADEvent * event;
        ADOutlierSSTD * outlier;
        ADio * io;

        int step = 0; 
        size_t idx_funcData = 0, idx_commData = 0;
        const unsigned long *funcData = nullptr, *commData = nullptr;
        //PQUEUE pq;

        // -----------------------------------------------------------------------
        // Measurement variables
        // -----------------------------------------------------------------------
        unsigned long total_frames = 0, frames = 0;
        unsigned long total_n_outliers = 0, n_outliers = 0;
        unsigned long total_processing_time = 0, processing_time = 0;
        unsigned long long n_func_events = 0, n_comm_events = 0;
        unsigned long long total_n_func_events = 0, total_n_comm_events = 0; 
        high_resolution_clock::time_point t1, t2;


        // -----------------------------------------------------------------------
        // Init. AD module
        // First, init io to make sure file (or connection) handler
        // -----------------------------------------------------------------------
        io = new ADio();
        io->setDispatcher();
        io->setHeader({{"rank", world_rank}, {"algorithm", 0}, {"nparam", 1}, {"winsz", 0}});
        // Note: currently, dump to disk is probablimatic on large scale. Considering overall chimuko architecture
        //       we want to go with data stream! For the test purpose, we don't dump or stream any data but just delete them.
        // io->open_curl(); // for VIS module
        io->open(output_dir + "/execdata." + std::to_string(world_rank), IOOpenMode::Write); // for file output

        // Second, init parser because it will hold shared memory with event and outlier object
        // also, process will be blocked at this line until it finds writer (in SST mode)
        parser = new ADParser(data_dir + "/" + inputFile, engineType);

        // Thrid, init event and outlier objects
        event = new ADEvent();
        event->linkFuncMap(parser->getFuncMap());
        event->linkEventType(parser->getEventType());

        outlier = new ADOutlierSSTD();
        outlier->linkExecDataMap(event->getExecDataMap());
        outlier->set_sigma(sigma);
        if (addr.length() > 0) {
#ifdef _USE_MPINET
            outlier->connect_ps(world_rank);
#else
            outlier->connect_ps(world_rank, 0, addr);
#endif
        }

        // -----------------------------------------------------------------------
        // Start analysis
        // -----------------------------------------------------------------------
        if (world_rank == 0) {
            std::cout << "rank: " << world_rank 
                    << " analysis start " << (outlier->use_ps() ? "with": "without") 
                    << " pserver" << std::endl;
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

            step = parser->getCurrentStep();
            parser->update_attributes();
            parser->fetchFuncData();
            parser->fetchCommData();

            n_func_events += (unsigned long long)parser->getNumFuncData();
            n_comm_events += (unsigned long long)parser->getNumCommData();

            idx_funcData = idx_commData = 0;
            funcData = nullptr;
            commData = nullptr;

            funcData = parser->getFuncData(idx_funcData);
            commData = parser->getCommData(idx_commData);
            while (funcData != nullptr || commData != nullptr)
            {
                // Determine event to handle
                // : if both, funcData and commData, are available, select one that occurs earlier.
                // : priority is on funcData because communication might happen within a function.
                const unsigned long* data = nullptr;
                bool is_func_event = true;
                if ( (funcData != nullptr && commData == nullptr) ) {
                    data = funcData;
                    is_func_event = true;
                }
                else if (funcData == nullptr && commData != nullptr) {
                    data = commData;
                    is_func_event = false;
                }
                else if (funcData[FUNC_IDX_TS] <= commData[COMM_IDX_TS]){
                    data = funcData;
                    is_func_event = true;                    
                }
                else {
                    data = commData;
                    is_func_event = false;
                }

                // Create event
                const Event_t ev = Event_t(
                    data, 
                    (is_func_event ? EventDataType::FUNC: EventDataType::COMM),
                    (is_func_event ? idx_funcData: idx_commData),
                    (
                        is_func_event ? 
                            generate_event_id(world_rank, step, idx_funcData, data[FUNC_IDX_F]):
                            generate_event_id(world_rank, step, idx_commData)
                    )
                );
                
                // Validate the event
                // NOTE: in SST mode with large rank number (>= 10), sometimes I got 
                // very large number for pid, rid and tid. This issue was also observed in python version.
                // Also, with BP mode, it doesn't have such problem. As temporal solution, we skip those 
                // data and it doesn't cause any problems (e.g. call stack violation). Need to consult with
                // adios team later.                
                if (!ev.valid() || ev.pid() > 1000000 || (int)ev.rid() != world_rank || ev.tid() >= 1000000)
                {
                    ; // do nothing
                }
                else 
                {
                    if (event->addEvent(ev) == EventError::CallStackViolation)
                    {
			; // do nothing
                        //std::cerr << "\n***** Call stack violation *****\n";
                        // ignore this particular event.
                        // it will possibly keep rasing call stack violation error.
                    }
                }
                
                // go next event
                if (is_func_event) {
                    idx_funcData++;
                    funcData = parser->getFuncData(idx_funcData);
                } else {
                    idx_commData++;
                    commData = parser->getCommData(idx_commData);
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
        if (world_rank == 0) {
            std::cout << "rank: " << world_rank << " analysis done!\n";
            event->show_status(true);
        }

        // -----------------------------------------------------------------------
        // Average analysis time and total number of outliers
        // -----------------------------------------------------------------------
        MPI_Barrier(MPI_COMM_WORLD);
        processing_time = duration_cast<milliseconds>(t2 - t1).count();

        {
            const unsigned long local_measures[] = {processing_time, n_outliers, frames};
            unsigned long global_measures[] = {0, 0, 0};
            MPI_Reduce(
                local_measures, global_measures, 3, MPI_UNSIGNED_LONG,
                MPI_SUM, 0, MPI_COMM_WORLD
            );
            total_processing_time = global_measures[0];
            total_n_outliers = global_measures[1];
            total_frames = global_measures[2];
        }
        {
            const unsigned long long local_measures[] = {n_func_events, n_comm_events};
            unsigned long long global_measures[] = {0, 0};
            MPI_Reduce(
                local_measures, global_measures, 2, MPI_UNSIGNED_LONG_LONG,
                MPI_SUM, 0, MPI_COMM_WORLD
            );
            total_n_func_events = global_measures[0];
            total_n_comm_events = global_measures[1];
        }

        
        if (world_rank == 0) {
            std::cout << "\n"
                << "Avg. num. frames     : " << (double)total_frames/(double)world_size << "\n"
                << "Avg. processing time : " << (double)total_processing_time/(double)world_size << " msec\n"
                << "Total num. outliers  : " << total_n_outliers << "\n"
                << "Total func events    : " << total_n_func_events << "\n"
                << "Total comm events    : " << total_n_comm_events << "\n"
                << "Total events         : " << total_n_func_events + total_n_comm_events
                << std::endl;
        }

        // -----------------------------------------------------------------------
        // Finalize
        // -----------------------------------------------------------------------
        outlier->disconnect_ps();

        delete parser;
        delete event;
        delete outlier;
        delete io;
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

/*
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

            // std::cout << "rank: " << world_rank << ", "
            //         << "Num. func events: " << parser->getNumFuncData() << ", "
            //         << "Num. comm events: " << parser->getNumCommData() << ", "
            //         << "Queue: " << pq.size() << std::endl;

            while (!pq.empty()) {
                const Event_t& ev = pq.top();
                // NOTE: in SST mode with large rank number (>= 10), sometimes I got 
                // very large number for pid, rid and tid. This issue is also observed in python version.
                // Also, with BP mode, it doesn't have such problem. As temporal solution, we skip those 
                // data and it doesn't cause any problems (e.g. call stack violation). Need to consult with
                // adios team later.
                if (!ev.valid() || ev.pid() != 0 || (int)ev.rid() != world_rank || ev.tid() >= 1000000)
                {
                    //std::cout << world_rank << "::::::" << ev << std::endl;
                    pq.pop();
                    continue;
                }
                if (event->addEvent(ev) == EventError::CallStackViolation)
                {
                    std::cerr << "\n***** Call stack violation *****\n";
                    exit(EXIT_FAILURE);
                }
                pq.pop();


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

*/
