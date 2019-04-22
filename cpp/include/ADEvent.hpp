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

    EventError addFunc(Event_t event, std::string event_id);
    EventError addComm(Event_t event);

private:
    void createCallStack(unsigned long pid, unsigned long rid, unsigned long tid);
    void createCallList(unsigned long pid, unsigned long rid, unsigned long tid);

private:
    const std::unordered_map<int, std::string> *m_funcMap;
    const std::unordered_map<int, std::string> *m_eventType;

    CallStackMap_p_t m_callStack;
    CallListMap_p_t  m_callList;
    CommListMap_p_t  m_commList;
    ExecDataMap_t    m_execDataMap;
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