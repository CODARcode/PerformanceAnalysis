#include "chimbuko/AD.hpp"

using namespace chimbuko;

int main(int argc, char ** argv)
{
    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);    

    std::string data_dir = "./data";
    std::string output_dir = "./data";
    std::string inputFile = "tau-metrics-" + std::to_string(world_rank) + ".bp";
    std::string engineType = "BPFile";

    double sigma = 6.0;

    ADParser * parser;
    ADEvent * event;
    ADOutlierSSTD * outlier;
    ADio * io;

    int step = 0; 
    unsigned long n_outliers;
    size_t idx_funcData = 0, idx_commData = 0, i = 0;
    const unsigned long *funcData = nullptr, *commData = nullptr;
    PQUEUE pq;

    parser = new ADParser(data_dir + "/" + inputFile, engineType);
    event = new ADEvent();
    outlier = new ADOutlierSSTD();
    io = new ADio();

    event->linkFuncMap(parser->getFuncMap());
    event->linkEventType(parser->getEventType());

    outlier->linkExecDataMap(event->getExecDataMap());
    outlier->set_sigma(sigma);
    outlier->connect_ps(world_rank);

    io->setWinSize(5);
    io->setDispatcher();
    io->setHeader({
        {"version", IO_VERSION}, {"rank", world_rank},
        {"algorithm", 0}, {"nparam", 1}, {"winsz", 5}
    });

    io->open_curl(); // for VIS module
    io->open(output_dir + "/execdata." + std::to_string(world_rank), IOOpenMode::Write); // for file output

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

        n_outliers = outlier->run();
        std::cout << n_outliers << std::endl;

        parser->endStep();

        io->write(event->trimCallList(), step);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    outlier->disconnect_ps();

    delete parser;
    delete event;
    delete outlier;
    delete io;

    return 0;
}