#pragma once
#include "ADDefine.hpp"
#include <iostream>
#include <list>
#include <stack>
#include <unordered_map>

class Event_t {
public:
    Event_t(const unsigned long * data, EventDataType t) : m_data(data), m_t(t) {}
    ~Event_t() {}

    // common for all kinds of events
    bool valid() const { return m_data != nullptr; }
    unsigned long pid() const { return m_data[IDX_P]; }
    unsigned long rid() const { return m_data[IDX_R]; }
    unsigned long tid() const { return m_data[IDX_T]; }
    unsigned long eid() const { return m_data[IDX_E]; }    
    unsigned long ts() const;
    
    EventDataType type() const { return m_t; }
    std::string strtype() const;


    // for function event
    unsigned long fid() const;

    // for communication event
    unsigned long tag() const;
    unsigned long partner() const;
    unsigned long bytes() const;

    // compare
    friend bool operator<(const Event_t& lhs, const Event_t& rhs);
    friend bool operator>(const Event_t& lhs, const Event_t& rhs);
    friend std::ostream& operator<<(std::ostream& os, const Event_t& ev);

private:
    const unsigned long * m_data;
    EventDataType m_t;
};

class CommData_t {
public:
    CommData_t() {};
    ~CommData_t() {};

private:
    std::string m_type;
    unsigned long m_src, m_tar, m_tid;
    unsigned long m_bytes, m_tag;
    unsigned long m_ts;
};

typedef std::list<CommData_t> CommList_t;
typedef CommList_t::iterator  CommListIterator_t;
typedef std::unordered_map<unsigned long, CommList_t>      CommListMap_t_t;
typedef std::unordered_map<unsigned long, CommListMap_t_t> CommListMap_r_t;
typedef std::unordered_map<unsigned long, CommListMap_r_t> CommListMap_p_t;

class ExecData_t {

public:
    ExecData_t(std::string id, Event_t& ev);
    ~ExecData_t();

    std::string get_id() const { return m_id; }
    unsigned long get_runtime() const { return m_runtime; }
    int get_label() const { return m_label; }
    
    void set_label(int label) { m_label = label; }
    void set_parent(std::string parent) { m_parent = parent; }
    void set_funcname(std::string funcname) { m_funcname = funcname; }

    bool update_exit(Event_t& ev);
    void add_child(std::string child);

    friend std::ostream& operator<<(std::ostream& os, const ExecData_t& exec);

private:
    std::string m_id;
    std::string m_funcname;
    unsigned long m_pid, m_tid, m_rid, m_fid;
    unsigned long m_entry, m_exit, m_runtime;
    int m_label;
    std::string m_parent;
    std::vector<std::string> m_children;
    std::vector<CommListIterator_t> m_messages;
};

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