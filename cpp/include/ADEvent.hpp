#pragma once
#include "ADDefine.hpp"
#include "ExecData.hpp"
#include <unordered_map>
#include <stack>
#include <list>


class ADEvent {
public:
    typedef std::list<ExecData_t> CallList_t;
    typedef CallList_t::iterator  CallListIterator_t;
    typedef std::unordered_map<unsigned long, CallList_t>      CallListMap_t_t;
    typedef std::unordered_map<unsigned long, CallListMap_t_t> CallListMap_r_t;
    typedef std::unordered_map<unsigned long, CallListMap_r_t> CallListMap_p_t;

    typedef std::stack<CallListIterator_t> CallStack_t;
    typedef std::unordered_map<unsigned long, CallStack_t>      CallStackMap_t_t;
    typedef std::unordered_map<unsigned long, CallStackMap_t_t> CallStackMap_r_t;
    typedef std::unordered_map<unsigned long, CallStackMap_r_t> CallStackMap_p_t;


public:
    ADEvent();
    ~ADEvent();

    void linkEventType(const std::unordered_map<int, std::string>* m) {
        m_eventType = m;
    }
    void linkFuncMap(const std::unordered_map<int, std::string>* m) {
        m_funcMap = m;
    }
    const std::unordered_map<int, std::string>* getFuncMap() const {
        return m_funcMap;
    }
    const std::unordered_map<int, std::string>* getEventType() const {
        return m_eventType;
    }

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
    std::unordered_map<unsigned long, std::vector<CallListIterator_t>> m_execDataMap;
};