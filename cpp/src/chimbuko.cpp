#include "chimbuko/chimbuko.hpp"

using namespace chimbuko;

Chimbuko::Chimbuko()
{
    m_parser = nullptr;
    m_event = nullptr;
    m_outlier = nullptr;
    m_io = nullptr;
}

Chimbuko::~Chimbuko()
{

}

void Chimbuko::init_io(int rank, std::string output_dir, 
    std::unordered_map<std::string, unsigned int> info, IOMode mode)
{
    // m_rank = rank;
    // m_output_dir = output_dir

    m_io = new ADio();
    m_io->setDispatcher();
    m_io->setHeader(info);
    //m_io->setHeader({{"rank", rank}, {"algorithm", 0}, {"nparam", 1}, {"winsz", 0}});
    // Note: currently, dump to disk is probablimatic on large scale. 
    // Considering overall chimuko architecture we want to go with data stream! 
    // For the test purpose, we don't dump or stream any data but just delete them.
    if (mode == IOMode::Online)
    {
        ;
        // m_io->open_curl(url); // for VIS module
    }
    else if (mode == IOMode::Offline)
    {
        m_io->open(
            output_dir + "/execdata." + std::to_string(rank),
            IOOpenMode::Write
        ); // for file output
    }
    else if (mode == IOMode::Both)
    {
        // m_io->open_curl(url); // for VIS module
        m_io->open(
            output_dir + "/execdata." + std::to_string(rank),
            IOOpenMode::Write
        ); // for file output
    }
}

void Chimbuko::init_parser(std::string data_dir, std::string inputFile, std::string engineType)
{
    // m_data_dir = data_dir;
    // m_inputFile = inputFile;
    // m_engineType = engineType;
    m_parser = new ADParser(data_dir + "/" + inputFile, engineType);
}

void Chimbuko::init_event(bool verbose)
{
    m_event = new ADEvent(verbose);
    m_event->linkFuncMap(m_parser->getFuncMap());
    m_event->linkEventType(m_parser->getEventType());
}

void Chimbuko::init_outlier(int rank, double sigma, std::string addr)
{
    m_outlier = new ADOutlierSSTD();
    m_outlier->linkExecDataMap(m_event->getExecDataMap());
    m_outlier->set_sigma(sigma);
    if (addr.length() > 0) {
#ifdef _USE_MPINET
        m_outlier->connect_ps(rank);
#else
        m_outlier->connect_ps(rank, 0, addr);
#endif
    }
}

void Chimbuko::finalize()
{
    m_outlier->disconnect_ps();
    if (m_parser) delete m_parser;
    if (m_event) delete m_event;
    if (m_outlier) delete m_outlier;
    if (m_io) delete m_io;

    m_parser = nullptr;
    m_event = nullptr;
    m_outlier = nullptr;
    m_io = nullptr;
}

void Chimbuko::run(int rank, 
    unsigned long long& n_func_events, 
    unsigned long long& n_comm_events,
    unsigned long& n_outliers,
    unsigned long& frames,
    bool only_one_frame,
    int interval_msec)
{
    int step = 0; 
    size_t idx_funcData = 0, idx_commData = 0;
    const unsigned long *funcData = nullptr, *commData = nullptr;

    while ( m_parser->getStatus() ) 
    {
        m_parser->beginStep();
        if (!m_parser->getStatus())
            break;

        // get trace data via SST
        step = m_parser->getCurrentStep();
        m_parser->update_attributes();
        m_parser->fetchFuncData();
        m_parser->fetchCommData();   

        // early SST buffer release
        m_parser->endStep();

        // count total number of events
        n_func_events += (unsigned long long)m_parser->getNumFuncData();
        n_comm_events += (unsigned long long)m_parser->getNumCommData();

        // prepare to analyze current step
        idx_funcData = idx_commData = 0;
        funcData = nullptr;
        commData = nullptr;

        funcData = m_parser->getFuncData(idx_funcData);
        commData = m_parser->getCommData(idx_commData);
        while (funcData != nullptr || commData != nullptr)
        {
            // Determine event to handle
            // - if both, funcData and commData, are available, select one that occurs earlier.
            // - priority is on funcData because communication might happen within a function.
            // - in the future, we may not need to process on commData (i.e. exclude it from execution data).
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
            else if (funcData[FUNC_IDX_TS] <= commData[COMM_IDX_TS]) {
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
                (is_func_event ? generate_event_id(rank, step, idx_funcData): "event_id")
            );

            // Validate the event
            // NOTE: in SST mode with large rank number (>=10), sometimes I got
            // very large number of pid, rid and tid. This issue was also observed in python version.
            // In BP mode, it doesn't have such problem. As temporal solution, we skip those data and
            // it doesn't cause any problems (e.g. call stack violation). Need to consult with 
            // adios team later
            if (!ev.valid() || ev.pid() > 1000000 || (int)ev.rid() != rank || ev.tid() >= 1000000)
            {
                std::cout << "\n***** Invalid event *****\n";
                std::cout << "[" << rank << "] " << ev << std::endl;
            }
            else
            {
                EventError err = m_event->addEvent(ev);
                // if (err == EventError::CallStackViolation)
                // {
                //     std::cout << "\n***** Call stack violation *****\n";
                // }
            }

            // go next event
            if (is_func_event) {
                idx_funcData++;
                funcData = m_parser->getFuncData(idx_funcData);
            } else {
                idx_commData++;
                commData = m_parser->getCommData(idx_commData);
            }
        }
        
        n_outliers += m_outlier->run(step);
        frames++;

        m_io->write(m_event->trimCallList(), step);

        if (only_one_frame)
            break;

        if (interval_msec)
            std::this_thread::sleep_for(std::chrono::microseconds(interval_msec));
    } // end of parser while loop
}
