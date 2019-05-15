#include "chimbuko/AD.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

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

    template <typename K, typename V>
    void show_map(const std::unordered_map<K, V>& m)
    {
        for (const auto& it : m)
        {
            std::cout << it.first << " : " << it.second << std::endl;
        }
    }    

    chimbuko::CallListMap_p_t* generate_callList(unsigned long max_calls) {
        using namespace chimbuko;

        const unsigned long N_FUNC = 10;
        const unsigned long N_EVENT = 3;
        const unsigned long N_TAGS  = 5;
        const unsigned long LABEL_STEP = 10;
        const unsigned long PARENT_STEP = 20;
        const unsigned long rid = (unsigned long)world_rank;

        CallListMap_p_t * callList = new CallListMap_p_t;

        for (unsigned long ts=0; ts < max_calls; ts++)
        {
            std::string fid = "Function_" + std::to_string(ts%N_FUNC);
            unsigned long entry_d[] = {0, rid, 0, ts%N_EVENT, ts%N_FUNC, ts};
            unsigned long exit_d[]  = {0, rid, 0, ts%N_EVENT, ts%N_FUNC, ts*10 + 1};

            Event_t entry_ev(entry_d, EventDataType::FUNC, FUNC_IDX_TS, fid);
            Event_t exit_ev(exit_d, EventDataType::FUNC, FUNC_IDX_TS, fid);
            ExecData_t exec(entry_ev);
            exec.update_exit(exit_ev);
            exec.set_funcname(fid + "_funcname");

            for (unsigned long i = 0; i < 5; i++) {
                exec.add_child(fid + "_child_" + std::to_string(i));

                unsigned long comm_d[] = {0, rid, 0, ts%(N_EVENT+2), ts%N_TAGS, (i+rid)%world_size, 10+i*1024, ts+1+i};
                Event_t comm_ev(comm_d, EventDataType::COMM, COMM_IDX_TS);
                exec.add_message(
                    CommData_t(comm_ev, ((i%2==0)? "SEND": "RECV"))
                );
            }

            if (ts%PARENT_STEP == 0) exec.set_parent(fid + "_parent");
            if (ts%LABEL_STEP == 0) exec.set_label(-1);

            (*callList)[0][world_rank][0].push_back(exec);
        }

        // std::cout << std::endl << (*callList)[0][world_rank][0].front() << std::endl;
        return callList;
    }
};

TEST_F(ADTest, EnvTest)
{
    EXPECT_EQ(4, world_size);
}

TEST_F(ADTest, BpfileTest)
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
        {134, 65, 83, 73, 77, 68, 34},
        { 89, 44, 62, 58, 54, 47, 39},
        { 99, 56, 59, 64, 66, 58, 43},
        {106, 58, 79, 49, 54, 66, 48}
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

    io->setDispatcher();
    io->setHeader({
        {"rank", world_rank}, {"algorithm", 0}, {"nparam", 1}, {"winsz", 5}
    });

    // in this test, we don't make any outputs
    //io->open_curl(); // for VIS module
    //io->open(output_dir + "/execdata." + std::to_string(world_rank), IOOpenMode::Write); // for file output

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

        io->write(event->trimCallList(), step);
    }
    EXPECT_EQ(N_STEPS[world_rank], step);

    MPI_Barrier(MPI_COMM_WORLD);

    delete parser;
    delete event;
    delete outlier;
    delete io;
}

#ifdef _USE_MPINET
TEST_F(ADTest, BpfileWithMpiNetTest)
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
        {134, 50, 65, 54, 62, 54, 31},
        { 75, 20, 33, 42, 32, 47, 45},
        { 72, 34, 27, 25, 28, 28, 36},
        { 76, 32, 47, 26, 26, 32, 27}
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
    outlier->connect_ps(world_rank);

    io->setDispatcher();
    io->setHeader({{"rank", world_rank},{"algorithm", 0}, {"nparam", 1}, {"winsz", 5}});

    // in this test, we don't make any outputs
    //io->open_curl(); // for VIS module
    //io->open(output_dir + "/execdata." + std::to_string(world_rank), IOOpenMode::Write); // for file output

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

        // for the test pupose, we stricly order the mpi processors;
        int token;
        if (world_rank != 0) {
            MPI_Recv(&token, 1, MPI_INT, world_rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        } else {
            token = 1;
        }

        n_outliers = outlier->run();
        EXPECT_EQ(N_OUTLIERS[world_rank][step], n_outliers);
        //std::cout << "step: " << step << ", rank: " << world_rank << ", N: " << n_outliers << std::endl;

        MPI_Send(&token, 1, MPI_INT, (world_rank+1)%world_size, 0, MPI_COMM_WORLD);
        if (world_rank == 0) {
            MPI_Recv(&token, 1, MPI_INT, world_size-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }


        parser->endStep();

        io->write(event->trimCallList(), step);
    }
    EXPECT_EQ(N_STEPS[world_rank], step);

    MPI_Barrier(MPI_COMM_WORLD);
    outlier->disconnect_ps();

    delete parser;
    delete event;
    delete outlier;
    delete io;
}

TEST_F(ADTest, ioReadWriteBinaryTest)
{
    using namespace chimbuko;
    const long long MAX_STEPS = 10;
    const unsigned long MAX_CALLS = 200;

    // Write
    ADio * ioW;
    ioW = new ADio();

    ioW->setDispatcher();
    ioW->setHeader({
        {"rank", world_rank}, {"algorithm", 0}, {"nparam", 1}, {"winsz", 3}
    });
    ioW->setLimit(400000); // 400k
    ioW->open("./temp/testexec." + std::to_string(world_rank), IOOpenMode::Write);

    for (long long step = 0; step < MAX_STEPS; step++) {
        CallListMap_p_t* callList = generate_callList(MAX_CALLS);
        ioW->write(callList, step);
    }

    EXPECT_EQ(3, ioW->getWinSize());

    // wait until all io jobs are finished
    for (int itry=0; itry<10; itry++) {
        while (ioW->getNumIOJobs()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    EXPECT_EQ(3, ioW->getDataFileIndex()+1);
    delete ioW;
    //MPI_Barrier(MPI_COMM_WORLD);

    // Read
    ADio * ioR;
    ioR = new ADio();

    ioR->setDispatcher(); // currently dispatcher can't be used for reading
    ioR->open("./temp/testexec." + std::to_string(world_rank), IOOpenMode::Read);
    EXPECT_EQ(world_rank, ioR->getHeader().get_rank());
    EXPECT_EQ((unsigned int)MAX_STEPS, ioR->getHeader().get_nframes());
    EXPECT_EQ(0, ioR->getHeader().get_algorithm());
    EXPECT_EQ(3, ioR->getHeader().get_winsize());
    EXPECT_EQ(1, ioR->getHeader().get_nparam());
    EXPECT_EQ((size_t)MAX_STEPS, ioR->getHeader().get_datapos().size());

    CallListMap_p_t* callList = generate_callList(MAX_CALLS);
    CallList_t& cl_ref = (*callList)[0][world_rank][0];
    IOError err;
    for (size_t iframe = 0; iframe < ioR->getHeader().get_nframes(); iframe++) {
        //long long iframe = world_rank*2;
        CallList_t cl;
        err = ioR->read(cl, (long long)iframe);
        if (err != IOError::OK)
            FAIL() << "Fail to get a frame.";

        EXPECT_EQ(137, cl.size());

        //EXPECT_TRUE(cl_ref.front().is_same(cl.front()));
        CallListIterator_t it = cl.begin(), it_ref = cl_ref.begin();
        while (it_ref != cl_ref.end() && it != cl.end()) {
            // the first one must be same
            ASSERT_TRUE(it_ref->is_same(*it));
            it_ref++;
            it++;

            if (it_ref == cl_ref.end()) break;
            if (it == cl.end()) break;

            // next 3 items are also same (winsz == 3) (right side)
            for (int i = 0; i < 3 && it_ref != cl_ref.end() && it != cl.end(); i++) {
                ASSERT_TRUE(it_ref->is_same(*it));
                it_ref++;
                it++;
            }

            if (it_ref == cl_ref.end()) break;
            if (it == cl.end()) break;

            // next 3 items were skipped.
            for (int i = 0; i < 3 && it_ref != cl_ref.end() && it != cl.end(); i++) {
                ASSERT_FALSE(it_ref->is_same(*it));
                it_ref++;
            }
            if (it_ref == cl_ref.end()) break;
            if (it == cl.end()) break;

            // next 3 items are also same (winsz == 3) (left side)
            for (int i = 0; i < 3  && it_ref != cl_ref.end() && it != cl.end(); i++) {                    
                ASSERT_TRUE(it_ref->is_same(*it));
                it_ref++;
                it++;
            }
        }
        ASSERT_TRUE(it == cl.end());

        int remained = 0;
        while (it_ref != cl_ref.end()) {
            it_ref++;
            remained++;
        }
        EXPECT_EQ(6, remained);
    }

    delete callList;
    delete ioR;
}

#endif