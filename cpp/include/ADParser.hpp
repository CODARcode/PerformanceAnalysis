#pragma once
#include <mpi.h>
#include <adios2.h>
#include <unordered_map>
#include "ADDefine.hpp"

class ADParser {

public:
    ADParser(std::string inputFile, std::string engineType="BPFile");
    ~ADParser();

    const std::unordered_map<int, std::string>* getFuncMap() const {
        return &m_funcMap;
    }
    const std::unordered_map<int, std::string>* getEventType() const {
        return &m_eventType;
    }

    bool getStatus() const { return m_status; }
    int getCurrentStep() const { return m_current_step; }

    int beginStep();
    void endStep();

    void update_attributes();
    ParserError fetchFuncData();
    ParserError fetchCommData();

    const unsigned long* getFuncData(size_t idx) const {
        if (idx >= m_timer_event_count) return nullptr;
        return &m_event_timestamps[idx * FUNC_EVENT_DIM];
    }
    size_t getNumFuncData() const { return m_timer_event_count; }
    
    const unsigned long* getCommData(size_t idx) const {
        if (idx >= m_comm_count) return nullptr;
        return &m_comm_timestamps[idx * COMM_EVENT_DIM];
    }
    size_t getNumCommData() const { return m_comm_count; }

private:
    adios2::ADIOS   m_ad;
    adios2::IO      m_io;
    adios2::Engine  m_reader;

    std::string m_inputFile;
    std::string m_engineType;
    bool m_status;
    bool m_opened;
    int  m_current_step;

    std::unordered_map<int, std::string> m_funcMap;
    std::unordered_map<int, std::string> m_eventType;

    size_t m_timer_event_count;
    std::vector<unsigned long> m_event_timestamps;

    size_t m_comm_count;
    std::vector<unsigned long> m_comm_timestamps;

};