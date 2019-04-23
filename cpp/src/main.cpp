#include "AD.hpp"
#include <chrono>
#include <queue>

#include "DispatchQueue.hpp"
#include <fstream>

typedef std::priority_queue<Event_t, std::vector<Event_t>, std::greater<std::vector<Event_t>::value_type>> PQUEUE;
template <typename K, typename V> void show_map(const std::unordered_map<K, V>& m);
int chimbuko(int argc, char ** argv);

void simple_task(std::ofstream& os) {
    os << "Dispatch 1!\n";
    // std::cout << "Dispatch 1!\n";
}

int main(int argc, char ** argv) {
    return chimbuko(argc, argv);
    // int r = 0;
    // std::ofstream myfile("/home/sungsooha/Desktop/CODAR/PerformanceAnalysis/cpp/test.txt", std::ios::app);

    // DispatchQueue q("Dispatch Queue Demo", 1);

    // // q.dispatch(&simple_task);
    // // q.dispatch(&simple_task);
    // // q.dispatch(&simple_task);
    // // q.dispatch(&simple_task);
    // q.dispatch([&myfile]{
    //     printf("Dispatch 2!\n");
    //     myfile << "Dispatch 2!\n";
    // });
    // q.dispatch([&myfile]{
    //     printf("Dispatch 3!\n");
    //     myfile << "Dispatch 3!\n";
    // });    
    // // q.dispatch([]{printf("Dispatch 3!\n");});
    // // q.dispatch([]{printf("Dispatch 4!\n");});

    // return r;
}


int chimbuko(int argc, char ** argv) {
    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    std::string data_dir = "/home/sungsooha/Desktop/CODAR/PerformanceAnalysis/data/mpi";
    std::string inputFile = "tau-metrics-0.bp";
    std::string engineType = "BPFile";

    ADParser * parser;
    ADEvent * event;
    ADOutlierSSTD * outlier;
    ADio * io;

    int step;
    size_t idx_funcData = 0, idx_commData = 0, i = 0;
    const unsigned long *funcData = nullptr, *commData = nullptr;
    PQUEUE pq;

    std::chrono::high_resolution_clock::time_point t1, t2;


    parser = new ADParser(data_dir + "/" + inputFile, engineType);
    event = new ADEvent();
    outlier = new ADOutlierSSTD();
    io = new ADio();

    event->linkFuncMap(parser->getFuncMap());
    event->linkEventType(parser->getEventType());
    outlier->linkExecDataMap(event->getExecDataMap());
    // io->linkCallListMap(event->getCallListMap());

    while (parser->getStatus())
    {
        parser->beginStep();
        if (!parser->getStatus())
        {
            std::cout << "Terminated with end of steps!" << std::endl;
            break; 
        }

        step = parser->getCurrentStep();
        std::cout << "Current step: " << step << std::endl;
        parser->update_attributes();
        // show_map<int, std::string>(*parser->getFuncMap());
        // show_map<int, std::string>(*event->getEventType());

        // todo: make simple class for performance measurement!
        t1 = std::chrono::high_resolution_clock::now();
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
            // if (ev.type() == EventDataType::FUNC) {
            //     std::cout << ev << ": " << evmap->find(ev.eid())->second;
            //     std::cout << ": " << funcmap->find(ev.fid())->second;
            //     std::cout << std::endl;            
            // } 
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
            // std::cout << ev << ": " << evmap->find(ev.eid())->second;
            // if (ev.type() == EventDataType::FUNC) {
            //     std::cout << ": " << funcmap->find(ev.fid())->second;
            // } else {
            //     std::cout << ": " << ev.partner() << ": " << ev.tag() << ": " << ev.bytes();
            // }
            // std::cout << std::endl;
        }

        std::cout << "# outliers: " << outlier->run() << std::endl;
        t2 = std::chrono::high_resolution_clock::now();
        std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " msec \n"; 

        // event->show_status();
        t1 = std::chrono::high_resolution_clock::now();
        // might be better to dump data after endStep() call (to free SST buffer asap)
        io->write(event->trimCallList());
        t2 = std::chrono::high_resolution_clock::now();
        std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " msec \n"; 

        // event->show_status(true);

        parser->endStep();


       // break;
    }

    delete parser;
    delete event;
    delete outlier;
    delete io;
    MPI_Finalize();
    return 0;
}

template <typename K, typename V>
void show_map(const std::unordered_map<K, V>& m)
{
    for (const auto& it : m)
    {
        std::cout << it.first << " : " << it.second << std::endl;
    }
}