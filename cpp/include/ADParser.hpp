#pragma once
#include <mpi.h>
#include <adios2.h>
#include <unordered_map>
#include "ADDefine.hpp"

class ADParser {
    public:
        ADParser(std::string inputFile, std::string engineType="BPFile");
        ~ADParser();

        bool getStatus() const { return m_status; }
        int getCurrentStep() const { return m_current_step; }

        int beginStep();
        void endStep();

        void update_attributes();
        ParserError getFuncData();
        ParserError getCommData();

        void show_funcMap() const;
        void show_eventType() const;

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