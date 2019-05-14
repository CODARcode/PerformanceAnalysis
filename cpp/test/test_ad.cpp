#include "chimbuko/AD.hpp"
#include <gtest/gtest.h>



class ADTest : public ::testing::Test
{
protected:
    int world_rank, world_size;

    virtual void SetUp()
    {
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
        MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    }

    virtual void TearDown()
    {

    }
};

TEST_F(ADTest, ADEnvTest)
{
    EXPECT_EQ(4, world_size);
}

TEST_F(ADTest, AdBpfileTest)
{
    using namespace chimbuko;
    //const int N_RANKS = 4;
    const std::vector<int> N_STEPS{6, 6, 6, 6};
    const std::vector<std::vector<size_t>> N_FUNC{
        {57077, 51845, 60561, 63278, 64628, 66484, 42233},
        {41215, 51375, 56620, 57683, 58940, 61010, 41963},
        {41108, 50820, 57237, 56590, 63458, 63931, 42486},
        {40581, 51127, 59175, 58158, 60465, 62516, 41238}
    }; 
    const std::vector<std::vector<size_t>> N_COMM{
        {89349, 107207, 121558, 123381, 128682, 131611, 83326},
        {78250, 94855 , 106301, 107222, 111581, 114273, 77817},
        {77020, 93355 , 105238, 106608, 114192, 118530, 79135},
        {77332, 93027 , 107856, 107628, 112019, 116468, 74517}
    };
    const std::vector<std::vector<unsigned long>> N_OUTLIERS{
        {134, 207, 272, 350, 424, 498, 540},
        { 89, 143, 206, 268, 325, 389, 435},
        { 99, 161, 224, 278, 385, 390, 425},
        {106, 161, 265, 294, 386, 455, 515}
    };


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

    io->setWinSize(5);
    io->setDispatcher();
    io->setHeader({
        {"version", IO_VERSION}, {"rank", world_rank},
        {"algorithm", 0}, {"nparam", 1}, {"winsz", 5}
    });

    //io->open_curl(); // for VIS module
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

        EXPECT_EQ(N_FUNC[world_rank][step], parser->getNumFuncData());
        EXPECT_EQ(N_COMM[world_rank][step], parser->getNumCommData());

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
        EXPECT_EQ(N_OUTLIERS[world_rank][step], n_outliers);

        parser->endStep();

        // fixbug!!!!
        //io->write(event->trimCallList(), step);
    }
    EXPECT_EQ(N_STEPS[world_rank], step);

    delete parser;
    delete event;
    delete outlier;
    delete io;
}