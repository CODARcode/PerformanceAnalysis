#include "chimbuko/ad/ADParser.hpp"
#include <thread>
#include <chrono>
#include <iostream>

using namespace chimbuko;

ADParser::ADParser(std::string inputFile, std::string engineType)
    : m_engineType(engineType), m_status(false), m_opened(false), m_attr_once(false), m_current_step(-1)
{
    m_inputFile = inputFile;
    m_ad = adios2::ADIOS(MPI_COMM_SELF, adios2::DebugON);

    // set io and engine
    m_io = m_ad.DeclareIO("tau-metrics");
    m_io.SetEngine(m_engineType);
    m_io.SetParameters({
        {"MarshalMethod", "BP"},
        {"DataTransport", "RDMA"}
    });
    
    // open file
    // for sst engine, is the adios2 internally blocked here until *.sst file is found?
    m_reader = m_io.Open(m_inputFile, adios2::Mode::Read, MPI_COMM_SELF);

    m_opened = true;
    m_status = true;
}

ADParser::~ADParser() {
    if (m_opened) {
        m_reader.Close();
    }
}

int ADParser::beginStep(bool verbose) {
    if (m_opened)
    {
        const int max_tries = 10000;
        int n_tries = 0;
        adios2::StepStatus status;
        while (n_tries < max_tries)
        {
            status = m_reader.BeginStep(adios2::StepMode::NextAvailable, 10.0f);
            if (status == adios2::StepStatus::NotReady)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                n_tries++;
                continue;
            }
            else if (status == adios2::StepStatus::OK)
            {
                m_current_step++;
                break;
            }
            else
            {
                m_status = false;
                m_current_step = -1;
                break;
            }
        }

        if (verbose)
        {
            std::cout << m_current_step << ": after " << n_tries << std::endl;
        }
    }
    return m_current_step;
}

void ADParser::endStep() {
    if (m_opened) {
        m_reader.EndStep();
    }
}

void ADParser::update_attributes() {
    if (!m_opened) return;
    if (m_engineType == "BPFile" && m_attr_once) return;

    const std::map<std::string, adios2::Params> attributes = m_io.AvailableAttributes();
    for (const auto attributePair: attributes)
    {
        std::string name = attributePair.first;
        bool is_func = name.find("timer") != std::string::npos;
        bool is_event = name.find("event_type") != std::string::npos;

        if (!is_func && !is_event) {
            // std::cout << name << ": " << attributePair.second.find("Value")->second << std::endl;
            continue;
        }

        int key = std::stoi(name.substr(name.find(" ")));
        std::unordered_map<int, std::string>& m = (is_func) ? m_funcMap: m_eventType;
        if (m.count(key) == 0 && attributePair.second.count("Value"))
        {
            std::string value = attributePair.second.find("Value")->second;
            size_t idx = 0;
            while ( (idx = value.find("\"")) != std::string::npos )
                value.replace(idx, 1, "");
            m[key] = value;
        }
    }
    m_attr_once = true;
}

ParserError ADParser::fetchFuncData() {
    adios2::Variable<size_t> in_timer_event_count;
    adios2::Variable<unsigned long> in_event_timestamps;
    size_t nelements;

    m_timer_event_count = 0;
    //m_event_timestamps.clear();

    in_timer_event_count = m_io.InquireVariable<size_t>("timer_event_count");
    in_event_timestamps = m_io.InquireVariable<unsigned long>("event_timestamps");

    if (in_timer_event_count && in_event_timestamps)
    {
        m_reader.Get<size_t>(in_timer_event_count, &m_timer_event_count, adios2::Mode::Sync);

        nelements = m_timer_event_count * FUNC_EVENT_DIM; 
        //m_event_timestamps.resize(nelements);
        if (nelements > m_event_timestamps.size())
            m_event_timestamps.resize(nelements);

        in_event_timestamps.SetSelection({{0, 0}, {m_timer_event_count, FUNC_EVENT_DIM}});
        m_reader.Get<unsigned long>(in_event_timestamps, m_event_timestamps.data(), adios2::Mode::Sync);
        return ParserError::OK;
    }
    return ParserError::NoFuncData;
}

ParserError ADParser::fetchCommData() {
    adios2::Variable<size_t> in_comm_count;
    adios2::Variable<unsigned long> in_comm_timestamps;
    size_t nelements;

    m_comm_count = 0;
    //m_comm_timestamps.clear();

    in_comm_count = m_io.InquireVariable<size_t>("comm_count");
    in_comm_timestamps = m_io.InquireVariable<unsigned long>("comm_timestamps");

    if (in_comm_count && in_comm_timestamps)
    {
        m_reader.Get<size_t>(in_comm_count, &m_comm_count, adios2::Mode::Sync);

        nelements = m_comm_count * COMM_EVENT_DIM;
        //m_comm_timestamps.resize(nelements);
        if (nelements > m_comm_timestamps.size())
            m_comm_timestamps.resize(nelements);

        in_comm_timestamps.SetSelection({{0, 0}, {m_comm_count, COMM_EVENT_DIM}});
        m_reader.Get<unsigned long>(in_comm_timestamps, m_comm_timestamps.data(), adios2::Mode::Sync);
        return ParserError::OK;
    }
    return ParserError::NoCommData;
}
