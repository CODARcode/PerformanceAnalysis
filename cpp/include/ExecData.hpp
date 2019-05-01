#pragma once
#include "ADDefine.hpp"
#include <iostream>
#include <string>
#include <vector>

namespace AD {

class Event_t {
public:
    Event_t(const unsigned long * data, EventDataType t, size_t idx, std::string id="event_id") 
        : m_data(data), m_t(t), m_id(id), m_idx(idx) {}
    ~Event_t() {}

    // common for all kinds of events
    bool valid() const { return m_data != nullptr; }
    std::string id() const { return m_id; }
    size_t idx() const { return m_idx; }
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
    std::string m_id;
    size_t m_idx;
};

bool operator<(const Event_t& lhs, const Event_t& rhs);
bool operator>(const Event_t& lhs, const Event_t& rhs);
std::ostream& operator<<(std::ostream& os, const Event_t& ev);


class CommData_t {
public:
    CommData_t();
    CommData_t(Event_t& ev, std::string commType);
    ~CommData_t() {};

    std::string type() const { return m_commType; }
    unsigned long ts() const { return m_ts; }
    unsigned long src() const { return m_src; }
    unsigned long tar() const { return m_tar; }

    void set_stream(bool is_binary) { m_is_binary = is_binary; }
    friend std::ostream& operator<<(std::ostream& os, CommData_t& comm);
    friend std::istream& operator>>(std::istream& is, CommData_t& comm);

private:
    std::string m_commType;
    unsigned long m_pid, m_rid, m_tid;
    unsigned long m_src, m_tar;
    unsigned long m_bytes, m_tag;
    unsigned long m_ts;
    bool m_is_binary;
};

std::ostream& operator<<(std::ostream& os, CommData_t& comm);
std::istream& operator>>(std::istream& is, CommData_t& comm);

class ExecData_t {

public:
    ExecData_t();
    ExecData_t(Event_t& ev);
    ~ExecData_t();

    std::string get_id() const { return m_id; }
    unsigned long get_entry() const { return m_entry; }
    unsigned long get_exit() const { return m_exit; }
    unsigned long get_runtime() const { return m_runtime; }
    int get_label() const { return m_label; }

    size_t get_n_message() const { return m_messages.size(); }
    size_t get_n_children() const { return m_children.size(); }

    bool is_used() const { return m_used; }

    const std::vector<std::string>& get_children() const { return m_children; }
    const std::vector<CommData_t>& get_message() const { return m_messages; }

    void set_label(int label) { m_label = label; }
    void set_parent(std::string parent) { m_parent = parent; }
    void set_funcname(std::string funcname) { m_funcname = funcname; }

    bool update_exit(Event_t& ev);
    void add_child(std::string child);
    bool add_message(const CommData_t& comm);

    void set_use(bool use) { m_used = use; }
    void set_stream(bool is_binary) { m_is_binary = is_binary; }
    friend std::ostream& operator<<(std::ostream& os, ExecData_t& exec);
    friend std::istream& operator>>(std::istream& is, ExecData_t& exec);

private:
    std::string m_id;
    std::string m_funcname;
    unsigned long m_pid, m_tid, m_rid, m_fid;
    unsigned long m_entry, m_exit, m_runtime;
    int m_label;
    std::string m_parent;
    std::vector<std::string> m_children;
    std::vector<CommData_t> m_messages;

    bool m_is_binary;
    bool m_used;
};

std::ostream& operator<<(std::ostream& os, ExecData_t& exec);
std::istream& operator>>(std::istream& is, ExecData_t& exec);
} // end of AD namespace