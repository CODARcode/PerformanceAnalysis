#pragma once
#include "chimbuko/ad/ADDefine.hpp"
#include <iostream>
#include <string>
#include <vector>

namespace chimbuko {

class Event_t {
public:
    Event_t(const unsigned long * data, EventDataType t, size_t idx, std::string id="event_id") 
        : m_data(data), m_t(t), m_id(id), m_idx(idx) {}
    ~Event_t() {}

    // common for all kinds of events
    bool valid() const { 
        return m_data != nullptr; 
    }
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
    CommData_t(const Event_t& ev, std::string commType);
    ~CommData_t() {};

    std::string type() const { return m_commType; }
    unsigned long ts() const { return m_ts; }
    unsigned long src() const { return m_src; }
    unsigned long tar() const { return m_tar; }

    void set_fid(unsigned long fid) { m_fid = fid; }
    void set_fname(std::string fname) { m_fname = fname; }
    void set_stream(bool is_binary) { m_is_binary = is_binary; }

    bool is_same(const CommData_t& other) const;

    // todo: need to review!!!
    friend std::ostream& operator<<(std::ostream& os, CommData_t& comm);
    friend std::istream& operator>>(std::istream& is, CommData_t& comm);

private:
    std::string m_commType;
    unsigned long m_pid, m_rid, m_tid;
    unsigned long m_src, m_tar;
    unsigned long m_bytes, m_tag;
    unsigned long m_ts;
    unsigned long m_fid;
    std::string m_fname;
    bool m_is_binary;
};

std::ostream& operator<<(std::ostream& os, CommData_t& comm);
std::istream& operator>>(std::istream& is, CommData_t& comm);

class ExecData_t {

public:
    ExecData_t();
    ExecData_t(const Event_t& ev);
    ~ExecData_t();

    std::string get_id() const { return m_id; }
    std::string get_funcname() const { return m_funcname; }
    unsigned long get_pid() const { return m_pid; }
    unsigned long get_tid() const { return m_tid; }
    unsigned long get_rid() const { return m_rid; }
    unsigned long get_fid() const { return m_fid; }
    long get_entry() const { return m_entry; }
    long get_exit() const { return m_exit; }
    long get_runtime() const { return m_runtime; }
    long get_inclusive() const { return m_runtime; }
    long get_exclusive() const { return m_exclusive; }
    int get_label() const { return m_label; }
    std::string get_parent() const { return m_parent; }
    //const std::vector<std::string>& get_children() const { return m_children; }
    //const std::vector<CommData_t>& get_message() const { return m_messages; }

    //size_t get_n_message() const { return m_messages.size(); }
    unsigned long get_n_message() const { return m_n_messages; }
    //size_t get_n_children() const { return m_children.size(); }
    unsigned long get_n_children() const { return m_n_children; }

    bool is_used() const { return m_used; }

    void set_label(int label) { m_label = label; }
    void set_parent(std::string parent) { m_parent = parent; }
    void set_funcname(std::string funcname) { m_funcname = funcname; }

    bool update_exit(const Event_t& ev);
    void update_exclusive(long t) { m_exclusive -= t; }
    void inc_n_children() { m_n_children++; }
    //void add_child(std::string child);
    bool add_message(CommData_t& comm);

    void set_use(bool use) { m_used = use; }
    void set_stream(bool is_binary) { m_is_binary = is_binary; }

    bool is_same(const ExecData_t& other) const;
    
    // todo: need to update!!!!!
    friend std::ostream& operator<<(std::ostream& os, ExecData_t& exec);
    friend std::istream& operator>>(std::istream& is, ExecData_t& exec);

private:
    std::string m_id;
    std::string m_funcname;
    unsigned long m_pid, m_tid, m_rid, m_fid;
    long m_entry, m_exit, m_runtime, m_exclusive;
    int m_label;
    std::string m_parent;
    unsigned long m_n_children;
    unsigned long m_n_messages;
    //std::vector<std::string> m_children;
    std::vector<CommData_t> m_messages;

    bool m_is_binary;
    bool m_used;
};

std::ostream& operator<<(std::ostream& os, ExecData_t& exec);
std::istream& operator>>(std::istream& is, ExecData_t& exec);
} // end of AD namespace