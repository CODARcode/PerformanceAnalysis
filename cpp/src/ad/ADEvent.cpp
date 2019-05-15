#include "chimbuko/ad/ADEvent.hpp"
#include <iostream>

using namespace chimbuko;

ADEvent::ADEvent() : m_funcMap(nullptr), m_eventType(nullptr) {

}

ADEvent::~ADEvent() {
    clear();
}

void ADEvent::clear() {
}

void ADEvent::createCommStack(unsigned long pid, unsigned long rid, unsigned long tid) {
    if (m_commStack.count(pid) == 0)
        m_commStack[pid] = CommStackMap_r_t();

    if (m_commStack[pid].count(rid) == 0)
        m_commStack[pid][rid] = CommStackMap_t_t();

    if (m_commStack[pid][rid].count(tid) == 0)
        m_commStack[pid][rid][tid] = CommStack_t();
}

void ADEvent::createCallStack(unsigned long pid, unsigned long rid, unsigned long tid) {
    if (m_callStack.count(pid) == 0)
        m_callStack[pid] = CallStackMap_r_t();

    if (m_callStack[pid].count(rid) == 0)
        m_callStack[pid][rid] = CallStackMap_t_t();

    if (m_callStack[pid][rid].count(tid) == 0)
        m_callStack[pid][rid][tid] = CallStack_t();
}

void ADEvent::createCallList(unsigned long pid, unsigned long rid, unsigned long tid) {
    if (m_callList.count(pid) == 0)
        m_callList[pid] = CallListMap_r_t();
    
    if (m_callList[pid].count(rid) == 0)
        m_callList[pid][rid] = CallListMap_t_t();

    if (m_callList[pid][rid].count(tid) == 0)
        m_callList[pid][rid][tid] = CallList_t();
}

EventError ADEvent::addEvent(Event_t event) {
    switch (event.type())
    {
    case EventDataType::FUNC: return addFunc(event);
    case EventDataType::COMM: return addComm(event);    
    default: return EventError::UnknownEvent;
    }
}

EventError ADEvent::addFunc(Event_t event) {
    if (m_eventType == nullptr) {
        //std::cerr << "Uninitialized eventType\n";
        return EventError::UnknownEvent;
    }

    std::string eventType;
    int eid = static_cast<int>(event.eid());
    if (m_eventType->count(eid) == 0) {
        //std::cerr << "Unknown event in eventType: " << eid << std::endl;
        return EventError::UnknownEvent;
    }

    this->createCallStack(event.pid(), event.rid(), event.tid());
    this->createCallList(event.pid(), event.rid(), event.tid());
    this->createCommStack(event.pid(), event.rid(), event.tid());

    eventType = m_eventType->find(eid)->second;
    
    if (eventType.compare("ENTRY") == 0)
    {
        CallList_t& cl = m_callList[event.pid()][event.rid()][event.tid()];
        cl.push_back(ExecData_t(event));

        CallListIterator_t it = std::prev(cl.end());
        CallStack_t& cs = m_callStack[event.pid()][event.rid()][event.tid()];
        if (cs.size()) {
            it->set_parent(cs.top()->get_id());
            cs.top()->add_child(it->get_id());
        }
        it->set_funcname(m_funcMap->find(event.fid())->second);
        cs.push(it);

        return EventError::OK;
    }
    else if (eventType.compare("EXIT") == 0)
    {
        CallStack_t& cs = m_callStack[event.pid()][event.rid()][event.tid()];
        if (cs.size() == 0) {
            std::cerr << "\n***** Empty call stack! *****\n" << std::endl;
            return EventError::CallStackViolation;
        }

        CallListIterator_t& it = cs.top();
        cs.pop();

        if (!it->update_exit(event)) {
            std::cerr << "\n***** Invalid EXIT event! *****\n" << std::endl;
            std::cerr << event << ": "
                      << m_eventType->find(event.eid())->second << ": "
                      << m_funcMap->find(event.fid())->second << std::endl;
            std::cerr << *it << std::endl;
            while (!cs.empty()) {
                std::cerr << *cs.top() << std::endl;
                cs.pop();
            }            
            return EventError::CallStackViolation;
        }

        CommStack_t& comm = m_commStack[event.pid()][event.rid()][event.tid()];
        while (!comm.empty()) {
            if (!it->add_message(comm.top()))
                break;
            comm.pop();
        }

        // todo: update depth (is this needed?)
        m_execDataMap[event.fid()].push_back(it);

        return EventError::OK;
    }

    return EventError::UnknownEvent;
}

EventError ADEvent::addComm(Event_t event) {
    if (m_eventType == nullptr)
        return EventError::UnknownEvent;

    int eid = static_cast<int>(event.eid());
    if (m_eventType->count(eid) == 0)
        return EventError::UnknownEvent;

    std::string eventType = m_eventType->find(eid)->second;

    if (eventType.compare("SEND") == 0 || eventType.compare("RECV") == 0) {
        this->createCommStack(event.pid(), event.rid(), event.tid());
        CommStack_t& cs = m_commStack[event.pid()][event.rid()][event.tid()];
        cs.push(CommData_t(event, eventType));
    }
    else {
        // std::cout << eventType << std::endl;
        return EventError::UnknownEvent;
    }
    
    return EventError::OK;
}

template <typename T>
static unsigned long nested_map_size(const T& m) {
    size_t n_elements = 0;

    for (auto it: m) 
        for (auto itt: it.second) 
            for (auto ittt: itt.second) 
                n_elements += ittt.second.size();

    return n_elements;
}

CallListMap_p_t* ADEvent::trimCallList() {
    CallListMap_p_t* cpListMap = new CallListMap_p_t;
    for (auto& it_p : m_callList) {
        for (auto& it_r : it_p.second) {
            for (auto& it_t: it_r.second) {
                CallList_t& cl = it_t.second;
                CallList_t cpList;

                auto it = cl.begin();
                while (it != cl.end()) {
                    // it = (it->get_runtime() < MAX_RUNTIME) ? cl.erase(it): ++it;
                    if (it->get_runtime() < MAX_RUNTIME) {
                        cpList.push_back(*it);
                        it = cl.erase(it);
                    }
                    else {
                        it++;
                    }                    
                }
                if (cpList.size())
                    (*cpListMap)[it_p.first][it_r.first][it_t.first] = cpList;
            }
        }
    }    
    m_execDataMap.clear();
    return cpListMap;
}

void ADEvent::show_status(bool verbose) const {
    size_t n_comm_remained = nested_map_size<CommStackMap_p_t>(m_commStack);
    size_t n_exec_remained = nested_map_size<CallStackMap_p_t>(m_callStack);
    size_t n_exec = nested_map_size<CallListMap_p_t>(m_callList);
    
    std::cout << "***** EVENT STATUS *****" << std::endl;
    std::cout << "Num. comm (remained): " << n_comm_remained << std::endl;
    std::cout << "Num. exec (remained): " << n_exec_remained << std::endl;
    std::cout << "Num. exec (total)   : " << n_exec << std::endl;
    if (verbose) {
        for (auto it : m_execDataMap) {
            std::cout << "Function: " << m_funcMap->find(it.first)->second << std::endl;
            for (auto itt: it.second) {
                if (itt->get_n_message() > 0 && itt->get_n_children())
                    std::cout << *itt << std::endl;
            }
        }
    }
    std::cout << "***** END   STATUS *****" << std::endl;
}