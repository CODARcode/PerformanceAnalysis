#include "ADParser.hpp"

int main(int argc, char ** argv) {
    MPI_Init(&argc, &argv);

    std::string data_dir = "/home/sungsooha/Desktop/CODAR/PerformanceAnalysis/data/mpi";
    std::string inputFile = "tau-metrics-0.bp";
    std::string engineType = "BPFile";

    ADParser* parser = new ADParser(data_dir + "/" + inputFile, engineType);
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
        // parser->show_funcMap();
        // parser->show_eventType();

        if (parser->getFuncData() != ParserError::OK)
        {
            std::cout << "No Function data!" << std::endl;
        }

        if (parser->getCommData() != ParserError::OK)
        {
            std::cout << "No Communication data!" << std::endl;
        }
        parser->endStep();
    }

    delete parser;
    MPI_Finalize();
    return 0;
}