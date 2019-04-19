#pragma once
#include <vector>
#include <string>
#include <iostream>

#define FUNC_EVENT_DIM 6
#define FUNC_IDX_P 0
#define FUNC_IDX_R 1
#define FUNC_IDX_T 2
#define FUNC_IDX_E 3
#define FUNC_IDX_F 4
#define FUNC_IDX_TS 5

#define COMM_EVENT_DIM 8

#define LEN_EVENT_ID 5

enum class ParserError
{
    OK = 0,
    NoFuncData = 1,
    NoCommData = 2
};

enum class EventError
{
    OK = 0,
    UnknownEvent = 1,
    CallStackViolation = 2
};


// typedef struct CommData {
//     std::string m_type;
//     unsigned long m_src, m_tar, m_tid;
//     unsigned long m_msg_size, m_msg_tag;
//     unsigned long m_ts;
// } CommData_t;

// typedef struct ExecData {
//     std::string m_id;
//     unsigned long m_pid, m_tid, m_rid, m_fid;
//     unsigned long m_entry, m_runtime;
//     int m_label;
//     std::string m_parent;
//     std::vector<std::string> m_children;
//     std::vector<CommData_t*> m_messages;
//     ExecData(std::string id, FuncEvent_t& ev) 
//         : m_id(id), m_runtime(0), m_label(1), m_parent("-1")  
//     {
//         m_pid = ev.pid();
//         m_rid = ev.rid();
//         m_tid = ev.tid();
//         m_fid = ev.fid();
//         m_entry = ev.ts();
//     }
//     std::string get_id() const { 
//         return m_id;
//     }
//     bool update_exit(FuncEvent_t& ev)
//     {
//         if (m_fid != ev.fid() || m_entry > ev.ts())
//             return false;
//         m_runtime = ev.ts() - m_entry;
//         return true;
//     }
//     void set_label(int label)
//     {
//         m_label = label;
//     }
//     void set_parent(std::string parent)
//     {
//         m_parent = parent;
//     }
//     void add_child(std::string child)
//     {
//         m_children.push_back(child);
//     }
// } ExecData_t;