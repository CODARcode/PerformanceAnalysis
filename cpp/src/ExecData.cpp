#include "ExecData.hpp"

ExecData_t::ExecData_t(std::string id, FuncEvent_t& ev) 
    : m_id(id), m_runtime(0), m_label(1), m_parent("-1")
{
    m_pid = ev.pid();
    m_rid = ev.rid();
    m_tid = ev.tid();
    m_fid = ev.fid();
    m_entry = ev.ts();    
}

ExecData_t::~ExecData_t() {

}

bool ExecData_t::update_exit(FuncEvent_t& ev)
{
    if (m_fid != ev.fid() || m_entry > ev.ts())
        return false;
    m_exit = ev.ts();
    m_runtime = m_exit - m_entry;
    return true;
}

void ExecData_t::add_child(std::string child)
{
    m_children.push_back(child);
}

std::ostream& operator<<(std::ostream& os, const ExecData_t& exec)
{
    os << exec.m_id 
       << "\npid: " << exec.m_pid << ", rid: " << exec.m_rid << ", tid: " << exec.m_tid
       << "\nfid: " << exec.m_fid << ", name: " << exec.m_funcname << ", label: " << exec.m_label 
       << "\nentry: " << exec.m_entry << ", runtime: " << exec.m_runtime
       << "\nparent: " << exec.m_parent << ", # children: " << exec.m_children.size() 
       << ", # messages: " << exec.m_messages.size();
    return os;
}