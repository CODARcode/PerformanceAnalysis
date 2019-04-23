#include "AD.hpp"
#include <chrono>
#include <queue>

template <typename K, typename V>
void show_map(const std::unordered_map<K, V>& m)
{
    for (const auto& it : m)
    {
        std::cout << it.first << " : " << it.second << std::endl;
    }
}

int main(int argc, char ** argv) {
    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    std::string data_dir = "/home/sungsooha/Desktop/CODAR/PerformanceAnalysis/data/mpi";
    std::string inputFile = "tau-metrics-0.bp";
    std::string engineType = "BPFile";

    ADParser* parser = new ADParser(data_dir + "/" + inputFile, engineType);
    ADEvent* event = new ADEvent();
    ADOutlierSSTD * outlier = new ADOutlierSSTD();

    event->linkFuncMap(parser->getFuncMap());
    event->linkEventType(parser->getEventType());
    outlier->linkExecDataMap(event->getExecDataMap());

    while (parser->getStatus())
    {
        parser->beginStep();
        if (!parser->getStatus())
        {
            std::cout << "Terminated with end of steps!" << std::endl;
            break; 
        }

        int step = parser->getCurrentStep();
        std::cout << "Current step: " << step << std::endl;
        parser->update_attributes();
        // show_map<int, std::string>(*parser->getFuncMap());
        // show_map<int, std::string>(*event->getEventType());

        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
        parser->fetchFuncData();
        parser->fetchCommData();

        size_t n_funcData = parser->getNumFuncData();
        size_t n_commData = parser->getNumCommData();
        size_t idx_funcData = 0, idx_commData = 0;
        const unsigned long *funcData = nullptr, *commData = nullptr;

        // auto funcmap = parser->getFuncMap();
        // auto evmap = parser->getEventType();
        // std::cout << "***** TEST *****" << std::endl;
        // for (size_t i = 0; i < 500; i++) {
        //     Event_t ev = Event_t(parser->getFuncData(i), EventDataType::FUNC, i, generate_event_id(world_rank, step, i));
        //     std::cout << ev << ": " << evmap->find(ev.eid())->second;
        //     if (ev.type() == EventDataType::FUNC) {
        //         std::cout << ": " << funcmap->find(ev.fid())->second;
        //     } else {
        //         std::cout << ": " << ev.partner() << ": " << ev.tag() << ": " << ev.bytes();
        //     }
        //     std::cout << std::endl;
        // }
        // std::cout << "***** END TEST *****" << std::endl;

        std::priority_queue<Event_t, std::vector<Event_t>, std::greater<std::vector<Event_t>::value_type>> pq;
        // warm-up: currently, this is required to resolve the issue that `pthread_create` apprears 
        // at the beginning of the first frame even though it timestamp say it shouldn't be at the beginning.
        for (size_t i = 0; i < 10; i++) {
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
            
        event->show_status();
        // if (parser->fetchFuncData() == ParserError::OK || parser->fetchCommData() == ParserError::OK)
        // {
        //     std::cout << "Start process ..." << std::endl;
        //     std::cout << "# FuncData: " << parser->getNumFuncData() << std::endl;
        //     std::cout << "# CommData: " << parser->getNumCommData() << std::endl;

        //     size_t idx_funcData = 0, idx_commData = 0;
        //     const unsigned long *funcData = nullptr, *commData = nullptr;
        //     EventError err;
        //     std::string event_id = "event_id";

        //     while (
        //         (funcData = parser->getFuncData(idx_funcData)) != nullptr || 
        //         (commData = parser->getCommData(idx_commData)) != nullptr
        //     )
        //     {
        //         Event_t ev_func(funcData, FUNC_IDX_TS);
        //         Event_t ev_comm(commData, COMM_IDX_TS);
        //         if ( (ev_func.valid() && ev_comm.valid() && ev_func.ts() <= ev_comm.ts()) ) {
        //             std::cout << "Add funcData 1: " << idx_funcData << std::endl;
        //             event_id = generate_event_id(world_rank, parser->getCurrentStep(), idx_funcData);        
        //             err = event->addFunc(ev_func, event_id);
        //             idx_funcData++;
        //         }
        //         else if ( (ev_func.valid() && ev_comm.valid() && ev_func.ts() > ev_comm.ts())) {
        //             std::cout << "Add commData 1: " << idx_commData << std::endl;
        //             err = event->addComm(ev_comm);
        //             idx_commData++;
        //         } 
        //         else if ( ev_func.valid() ) {
        //             std::cout << "Add funcData 2: " << idx_funcData << std::endl;
        //             event_id = generate_event_id(world_rank, parser->getCurrentStep(), idx_funcData);        
        //             err = event->addFunc(ev_func, event_id);
        //             idx_funcData++;
        //         } 
        //         else if ( ev_comm.valid() ) {
        //             std::cout << "Add commData 2: " << idx_commData << std::endl;
        //             err = event->addComm(ev_comm);
        //             idx_commData++;
        //         } else {
        //             std::cout << "Error!!" << std::endl;
        //         }

        //         // event_id = generate_event_id(world_rank, parser->getCurrentStep(), idx_funcData);        
        //         // err = event->addFunc(FuncEvent_t(funcData), event_id);
        //         if (err == EventError::CallStackViolation)
        //         {
        //             std::cout << "\n*** Call stack violation ***\n";
        //             exit(EXIT_FAILURE);
        //         }
        //         // idx_funcData += 1;
        //     }   
        //     std::cout << "End of process." << std::endl;
        //     std::cout << "# FuncData: " << idx_funcData << std::endl;
        //     std::cout << "# CommData: " << idx_commData << std::endl;

        //     outlier->run();
        // }

        
        // else
        // {
        //     std::cout << "No Function data!" << std::endl;
        // }
        
        // if (parser->fetchCommData() == ParserError::OK)
        // {
        //     std::cout << "Handle Communication data!" << std::endl;
        //     EventError err;
        //     const unsigned long * commData = nullptr;
        //     size_t idx_commData = 0;

        //     while ((commData = parser->getCommData(idx_commData)) != nullptr)
        //     {
        //         err = event->addComm(CommEvent_t(commData));
        //         idx_commData += 1;
        //     }
        // }
        // else
        // {
        //     std::cout << "No Communication data!" << std::endl;            
        // }
        std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        std::cout << duration << " msec \n"; 
        
        parser->endStep();
        //break;
    }

    // show_map<int, std::string>(*parser->getEventType());
    // show_map<int, std::string>(*event->getEventType());

    // for (int i=0; i<5; i++)
    // {
    //     std::cout << generate_hex(5) << std::endl;
    // }
    // parser->show_eventType();
    // event->show_eventType();
    // event->show_funcMap();

    delete parser;
    delete event;
    delete outlier;
    MPI_Finalize();
    return 0;
}