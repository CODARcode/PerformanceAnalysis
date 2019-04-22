#include "AD.hpp"
#include <chrono>

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

    std::string data_dir = "/home/sungsooha/Desktop/CODAR/PerformanceAnalysis/data/mpi";
    std::string inputFile = "tau-metrics-0.bp";
    std::string engineType = "BPFile";

    ADParser* parser = new ADParser(data_dir + "/" + inputFile, engineType);
    ADEvent* event = new ADEvent();
    //ADOutlier* outlier = new ADOutlier();
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

        std::cout << "Current step: " << parser->getCurrentStep() << std::endl;
        parser->update_attributes();
        // show_map<int, std::string>(*parser->getFuncMap());
        // show_map<int, std::string>(*event->getEventType());

        if (parser->fetchFuncData() == ParserError::OK)
        {
            size_t idx_funcData = 0;
            const unsigned long * funcData = nullptr;
            EventError err;
            std::string event_id = "event_id";

            std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
            while ((funcData = parser->getFuncData(idx_funcData)) != nullptr )
            {
                // todo: generate event_id: [rank]_[step id]_[idx]
                //event_id = generate_hex(LEN_EVENT_ID);                
                err = event->addFunc(FuncEvent_t(funcData), event_id);
                if (err == EventError::CallStackViolation)
                {
                    std::cout << "\n*** Call stack violation ***\n";
                    exit(EXIT_FAILURE);
                }
                // std::cout << idx_funcData << ": ";
                // for (int i = 0; i < FUNC_EVENT_DIM; i++)
                //     std::cout << funcData[i] << " ";
                // std::cout << std::endl;
                idx_funcData += 1;

                // if (idx_funcData >= 10)
                //     break;
            }   
            std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
            std::cout << duration << " msec \n"; 

            outlier->run();
        }
        else
        {
            std::cout << "No Function data!" << std::endl;
        }
        


        if (parser->fetchCommData() != ParserError::OK)
        {
            std::cout << "No Communication data!" << std::endl;
        }
        parser->endStep();
        break;
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