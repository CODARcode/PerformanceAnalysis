#pragma once
#include "ADDefine.hpp"
#include <iostream>
#include <list>
#include <stack>
#include <unordered_map>

typedef struct FuncEvent {
    const unsigned long * m_data;
    FuncEvent() : m_data(nullptr) {}
    FuncEvent(const unsigned long * data) : m_data(data) {}
    unsigned long pid() { return m_data[FUNC_IDX_P]; }
    unsigned long rid() { return m_data[FUNC_IDX_R]; }
    unsigned long tid() { return m_data[FUNC_IDX_T]; }
    unsigned long eid() { return m_data[FUNC_IDX_E]; }
    unsigned long fid() { return m_data[FUNC_IDX_F]; }
    unsigned long ts() { return m_data[FUNC_IDX_TS]; }
} FuncEvent_t;

typedef struct {
    std::string m_type;
    unsigned long m_src, m_tar, m_tid;
    unsigned long m_msg_size, m_msg_tag;
    unsigned long m_ts;
} CommData_t;

class ExecData_t {

public:
    ExecData_t(std::string id, FuncEvent_t& ev);
    ~ExecData_t();

    std::string get_id() const { return m_id; }
    unsigned long get_runtime() const { return m_runtime; }
    void set_label(int label) { m_label = label; }
    void set_parent(std::string parent) { m_parent = parent; }
    void set_funcname(std::string funcname) { m_funcname = funcname; }

    bool update_exit(FuncEvent_t& ev);
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
    std::vector<CommData_t*> m_messages;
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