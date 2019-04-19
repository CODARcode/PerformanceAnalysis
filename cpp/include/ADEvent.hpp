#pragma once
#include "ADDefine.hpp"
#include "ExecData.hpp"

class ADEvent {

public:
    ADEvent();
    ~ADEvent();

    void linkEventType(const std::unordered_map<int, std::string>* m) { m_eventType = m; }
    void linkFuncMap(const std::unordered_map<int, std::string>* m) { m_funcMap = m; }
    const std::unordered_map<int, std::string>* getFuncMap() const { return m_funcMap; }
    const std::unordered_map<int, std::string>* getEventType() const { return m_eventType; }
    const ExecDataMap_t* getExecDataMap() const { return &m_execDataMap; }

    void clear();

    EventError addFunc(FuncEvent_t event, std::string event_id);

private:
    void createCallStack(unsigned long pid, unsigned long rid, unsigned long tid);
    void createCallList(unsigned long pid, unsigned long rid, unsigned long tid);

private:
    const std::unordered_map<int, std::string> *m_funcMap;
    const std::unordered_map<int, std::string> *m_eventType;

    CallStackMap_p_t m_callStack;
    CallListMap_p_t  m_callList;
    ExecDataMap_t    m_execDataMap;
};