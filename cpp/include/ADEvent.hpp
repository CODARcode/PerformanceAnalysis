#pragma once
#include "ADDefine.hpp"
#include "ExecData.hpp"
#include <list>
#include <stack>
#include <unordered_map>

typedef std::stack<CommData_t> CommStack_t;
typedef std::unordered_map<unsigned long, CommStack_t>      CommStackMap_t_t;
typedef std::unordered_map<unsigned long, CommStackMap_t_t> CommStackMap_r_t;
typedef std::unordered_map<unsigned long, CommStackMap_r_t> CommStackMap_p_t;

typedef std::list<ExecData_t> CallList_t;
typedef CallList_t::iterator  CallListIterator_t;
typedef std::unordered_map<unsigned long, CallList_t>      CallListMap_t_t;
typedef std::unordered_map<unsigned long, CallListMap_t_t> CallListMap_r_t;
typedef std::unordered_map<unsigned long, CallListMap_r_t> CallListMap_p_t;

typedef std::stack<CallListIterator_t> CallStack_t;
typedef std::unordered_map<unsigned long, CallStack_t>      CallStackMap_t_t;
typedef std::unordered_map<unsigned long, CallStackMap_t_t> CallStackMap_r_t;
typedef std::unordered_map<unsigned long, CallStackMap_r_t> CallStackMap_p_t;

typedef std::unordered_map<unsigned long, std::vector<CallListIterator_t>> ExecDataMap_t;

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

    EventError addEvent(Event_t event);
    EventError addFunc(Event_t event);
    EventError addComm(Event_t event);

    void show_status() const;

private:
    void createCommStack(unsigned long pid, unsigned long rid, unsigned long tid);
    void createCallStack(unsigned long pid, unsigned long rid, unsigned long tid);
    void createCallList(unsigned long pid, unsigned long rid, unsigned long tid);

private:
    const std::unordered_map<int, std::string> *m_funcMap;
    const std::unordered_map<int, std::string> *m_eventType;

    CommStackMap_p_t  m_commStack;
    CallStackMap_p_t  m_callStack;
    CallListMap_p_t   m_callList;
    ExecDataMap_t     m_execDataMap;
};

// pid, rid, tid, eid, tag, partner, nbytes, ts = event
// if eid not in [self.send, self.recv]:
//     if self.log is not None:
//         self.log.debug("No attributes for SEND/RECV")
//     return True

// try:
//     execData = self.funStack[pid][rid][tid][-1]
// except IndexError:
//     if self.log is not None:
//         self.log.debug("Communication event before any function calls")
//     return True
// except KeyError:
//     if self.log is not None:
//         self.log.debug("Communication event before any function calls")
//     return True

// if eid == self.send:
//     comType = 'send'
//     comSrc = rid
//     comDst = partner
// else:
//     comType = 'receive'
//     comSrc = partner
//     comDst = rid

// comData = CommData(comm_type=comType, src=comSrc, dst=comDst, tid=tid,
//                     msg_size=nbytes, msg_tag=tag, ts=ts)

// execData.add_message(comData)
// return True