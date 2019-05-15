#include "chimbuko/ad/ExecData.hpp"

using namespace chimbuko;

/* ---------------------------------------------------------------------------
 * Implementation of ExecData_t class
 * --------------------------------------------------------------------------- */
ExecData_t::ExecData_t()
    : m_runtime(MAX_RUNTIME), m_label(1), m_parent("-1"), m_is_binary(false), m_used(false)
{

}
ExecData_t::ExecData_t(Event_t& ev) 
    : m_runtime(MAX_RUNTIME), m_label(1), m_parent("-1"), m_is_binary(false), m_used(false)
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

bool ExecData_t::is_same(const ExecData_t& other) const {
    if (!(m_id == other.m_id)) return false;
    if (!(m_funcname == other.m_funcname)) return false;
    if (!(m_pid == other.m_pid)) return false;
    if (!(m_tid == other.m_tid)) return false;
    if (!(m_rid == other.m_rid)) return false;
    if (!(m_fid == other.m_fid)) return false;
    if (!(m_entry == other.m_entry)) return false;
    if (!(m_exit == other.m_exit)) return false;
    if (!(m_runtime == other.m_runtime)) return false;
    if (!(m_label == other.m_label)) return false;
    if (!(m_parent == other.m_parent)) return false;
    if (!(m_children.size() == other.m_children.size())) return false;
    if (!(m_messages.size() == other.m_messages.size())) return false;

    for (size_t i = 0; i < m_children.size(); i++)
        if (!(m_children[i] == other.m_children[i]))
            return false;

    for (size_t i = 0; i < m_messages.size(); i++)
        if (!(m_messages[i].is_same(other.m_messages[i])))
            return false;

    return true;
}

static void write_str(std::ostream& os, std::string& str) {
    // std::cout << str << ", " << str.size() << std::endl;
    size_t len = str.size();
    os.write((const char*)&len, sizeof(size_t));
    os.write((const char*)str.c_str(), len);
}

static std::string read_str(std::istream& is) {
    size_t len;
    std::string buffer;
    is.read((char*)&len, sizeof(size_t));

    buffer.resize(len);
    is.read((char*)&buffer[0], len);

    return buffer;
}

template <typename T>
static void write_num(std::ostream& os, T num) {
    // std::cout << num << ", " << sizeof(T) << std::endl;
    os.write((const char*)&num, sizeof(T));
}

template <typename T>
static T read_num(std::istream& is) {
    T num;
    is.read((char*)&num, sizeof(T));
    return num;
}

std::ostream& chimbuko::operator<<(std::ostream& os, ExecData_t& exec)
{
    if (exec.m_is_binary) {
        write_str(os, exec.m_id);
        write_str(os, exec.m_funcname);
        write_num<unsigned long>(os, exec.m_pid);
        write_num<unsigned long>(os, exec.m_tid);
        write_num<unsigned long>(os, exec.m_rid);
        write_num<unsigned long>(os, exec.m_fid);
        write_num<unsigned long>(os, exec.m_entry);
        write_num<unsigned long>(os, exec.m_exit);
        write_num<unsigned long>(os, exec.m_runtime);
        write_num<int>(os, exec.m_label);
        write_str(os, exec.m_parent);

        write_num<size_t>(os, exec.m_children.size());
        for (auto c: exec.m_children) { 
            write_str(os, c);
        }

        write_num<size_t>(os, exec.m_messages.size());
        for (auto m: exec.m_messages) {
            m.set_stream(true);
            os << m;
            m.set_stream(false);
        }
    }
    else {
        os << exec.m_id 
            << "\npid: " << exec.m_pid << ", rid: " << exec.m_rid << ", tid: " << exec.m_tid
            << "\nfid: " << exec.m_fid << ", name: " << exec.m_funcname << ", label: " << exec.m_label 
            << "\nentry: " << exec.m_entry << ", exit: " << exec.m_exit << ", runtime: " << exec.m_runtime
            << "\nparent: " << exec.m_parent << ", # children: " << exec.m_children.size() 
            << ", # messages: " << exec.m_messages.size();

        if (exec.m_children.size()) {
            os << "\nChildren: ";
            for (auto c: exec.m_children)
                os << c << ", ";
        }
        if (exec.m_messages.size()) {
            os << "\nMessage: \n";
            for (auto msg: exec.m_messages)
                os << msg << std::endl;
        }
    }
    return os;
}

std::istream& chimbuko::operator>>(std::istream& is, ExecData_t& exec)
{
    if (exec.m_is_binary) {
        exec.m_id = read_str(is);
        exec.m_funcname = read_str(is);
        exec.m_pid = read_num<unsigned long>(is);
        exec.m_tid = read_num<unsigned long>(is);
        exec.m_rid = read_num<unsigned long>(is);
        exec.m_fid = read_num<unsigned long>(is);
        exec.m_entry = read_num<unsigned long>(is);
        exec.m_exit = read_num<unsigned long>(is);
        exec.m_runtime = read_num<unsigned long>(is);
        exec.m_label = read_num<int>(is);
        exec.m_parent = read_str(is);

        size_t n_children = read_num<size_t>(is);
        exec.m_children.resize(n_children);
        for (size_t i = 0; i < n_children; i++) {
            exec.m_children[i] = read_str(is);
        }

        size_t n_msg = read_num<size_t>(is);
        exec.m_messages.resize(n_msg);
        for (size_t i = 0; i < n_msg; i++) {
            exec.m_messages[i].set_stream(true);
            is >> exec.m_messages[i];
            exec.m_messages[i].set_stream(false);
        }
    }
    else {
        std::cerr << "Currently, only binary format is supported for ExecData_t!" << std::endl;
        exit(EXIT_FAILURE);
    }
    return is;
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
        std::cerr << "\n***** It is NOT comm event and tried to get partner! *****\n";
        exit(EXIT_FAILURE);
    } 
    return m_data[COMM_IDX_PARTNER]; 
}
unsigned long Event_t::bytes() const { 
    if (m_t != EventDataType::COMM) {
        std::cerr << "\n***** It is NOT comm event and tried to get bytes! *****\n";
        exit(EXIT_FAILURE);
    } 
    return m_data[COMM_IDX_BYTES]; 
}

bool chimbuko::operator<(const Event_t& lhs, const Event_t& rhs) {
    if (lhs.ts() == rhs.ts()) return lhs.idx() < rhs.idx();
    return lhs.ts() < rhs.ts();
}

bool chimbuko::operator>(const Event_t& lhs, const Event_t& rhs) {
    if (lhs.ts() == rhs.ts()) return lhs.idx() > rhs.idx();
    return lhs.ts() > rhs.ts();
}

std::ostream& chimbuko::operator<<(std::ostream& os, const Event_t& ev) {
    os << ev.ts() << ": " << ev.strtype() << ": " 
       << ev.pid() << ": " << ev.rid() << ": " << ev.tid();
       
    return os;
}

/* ---------------------------------------------------------------------------
 * Implementation of CommData_t class
 * --------------------------------------------------------------------------- */
CommData_t::CommData_t() : m_is_binary(false)
{

}

CommData_t::CommData_t(Event_t& ev, std::string commType) 
: m_commType(commType), m_is_binary(false) 
{
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

bool CommData_t::is_same(const CommData_t& other) const 
{
    if (!(m_commType == other.m_commType)) return false;
    if (!(m_pid == other.m_pid)) return false;
    if (!(m_rid == other.m_rid)) return false;
    if (!(m_tid == other.m_tid)) return false;
    if (!(m_src == other.m_src)) return false;
    if (!(m_bytes == other.m_bytes)) return false;
    if (!(m_tag == other.m_tag)) return false;
    if (!(m_ts == other.m_ts)) return false;

    return true;
}

std::ostream& chimbuko::operator<<(std::ostream& os, CommData_t& comm) {
    if (comm.m_is_binary) {
        write_str(os, comm.m_commType);
        write_num<unsigned long>(os, comm.m_pid);
        write_num<unsigned long>(os, comm.m_rid);
        write_num<unsigned long>(os, comm.m_tid);
        write_num<unsigned long>(os, comm.m_src);
        write_num<unsigned long>(os, comm.m_tar);
        write_num<unsigned long>(os, comm.m_bytes);
        write_num<unsigned long>(os, comm.m_tag);
        write_num<unsigned long>(os, comm.m_ts);
    }
    else {
        os << comm.ts() << ": " << comm.type() << ": " << comm.src() << ": " << comm.tar();
    }
    return os;
}

std::istream& chimbuko::operator>>(std::istream& is, CommData_t& comm) {
    if (comm.m_is_binary) {
        comm.m_commType = read_str(is);
        comm.m_pid = read_num<unsigned long>(is);
        comm.m_rid = read_num<unsigned long>(is);
        comm.m_tid = read_num<unsigned long>(is);
        comm.m_src = read_num<unsigned long>(is);
        comm.m_tar = read_num<unsigned long>(is);
        comm.m_bytes = read_num<unsigned long>(is);
        comm.m_tag = read_num<unsigned long>(is);
        comm.m_ts = read_num<unsigned long>(is);
    }
    else {
        std::cerr << "Currently, only binary format is supported for CommData_t!" << std::endl;
        exit(EXIT_FAILURE);
    }
    return is;
}