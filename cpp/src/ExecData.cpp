#include "ExecData.hpp"

/* ---------------------------------------------------------------------------
 * Implementation of ExecData_t class
 * --------------------------------------------------------------------------- */
ExecData_t::ExecData_t(Event_t& ev) 
    : m_runtime(0), m_label(1), m_parent("-1")
{
    m_id = ev.id();
    m_pid = ev.pid();
    m_rid = ev.rid();
    m_tid = ev.tid();
    m_fid = ev.fid();
    m_entry = ev.ts();    
}

ExecData_t::~ExecData_t() {

}

bool ExecData_t::update_exit(Event_t& ev)
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

bool ExecData_t::add_message(const CommData_t& comm) {
    if (comm.ts() < m_entry || comm.ts() > m_exit)
        return false;
    m_messages.push_back(comm);
    return true;
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


/* ---------------------------------------------------------------------------
 * Implementation of Event_t class
 * --------------------------------------------------------------------------- */
unsigned long Event_t::ts() const {
    switch (m_t) {
        case EventDataType::FUNC: return m_data[FUNC_IDX_TS];
        case EventDataType::COMM: return m_data[COMM_IDX_TS];
        default: return UINT64_MAX;
    }     
}

std::string Event_t::strtype() const {
    switch(m_t) {
        case EventDataType::FUNC: return "FUNC";
        case EventDataType::COMM: return "COMM";
        case EventDataType::COUNT: return "COUNT";
        default: return "Unknown";
    }
}

// for function event
unsigned long Event_t::fid() const { 
    if (m_t != EventDataType::FUNC) {
        std::cerr << "\n***** It is NOT func event and tried to get fid! *****\n";
        exit(EXIT_FAILURE);
    }
    return m_data[FUNC_IDX_F]; 
}

// for communication event
unsigned long Event_t::tag() const {
    if (m_t != EventDataType::COMM) {
        std::cerr << "\n***** It is NOT comm event and tried to get tag! *****\n";
        exit(EXIT_FAILURE);
    } 
    return m_data[COMM_IDX_TAG]; 
}
unsigned long Event_t::partner() const { 
    if (m_t != EventDataType::COMM) {
        std::cerr << "\n***** It is NOT comm event and tried to get tag! *****\n";
        exit(EXIT_FAILURE);
    } 
    return m_data[COMM_IDX_PARTNER]; 
}
unsigned long Event_t::bytes() const { 
    if (m_t != EventDataType::COMM) {
        std::cerr << "\n***** It is NOT comm event and tried to get tag! *****\n";
        exit(EXIT_FAILURE);
    } 
    return m_data[COMM_IDX_BYTES]; 
}

bool operator<(const Event_t& lhs, const Event_t& rhs) {
    if (lhs.ts() == rhs.ts()) return lhs.idx() < rhs.idx();
    return lhs.ts() < rhs.ts();
}

bool operator>(const Event_t& lhs, const Event_t& rhs) {
    if (lhs.ts() == rhs.ts()) return lhs.idx() > rhs.idx();
    return lhs.ts() > rhs.ts();
}

std::ostream& operator<<(std::ostream& os, const Event_t& ev) {
    os << ev.ts() << ":" << ev.strtype() << ": " 
       << ev.pid() << ": " << ev.rid() << ": " << ev.tid();
       
    return os;
}

/* ---------------------------------------------------------------------------
 * Implementation of CommData_t class
 * --------------------------------------------------------------------------- */
CommData_t::CommData_t(Event_t& ev, std::string commType) : m_commType(commType) {
    m_pid = ev.pid();
    m_rid = ev.rid();
    m_tid = ev.tid();
    
    if (m_commType.compare("SEND") == 0) {
        m_src = m_rid;
        m_tar = ev.partner();
    } else if (m_commType.compare("RECV") == 0){
        m_src = ev.partner();
        m_tar = m_rid;
    }

    m_bytes = ev.bytes();
    m_tag = ev.tag();
    m_ts = ev.ts();
}

std::ostream& operator<<(std::ostream& os, const CommData_t& comm) {
    os << comm.ts() << ": " << comm.type() << ": " << comm.src() << ": " << comm.tar();
    return os;
}