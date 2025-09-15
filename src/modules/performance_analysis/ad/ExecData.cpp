#include "chimbuko/modules/performance_analysis/ad/ExecData.hpp"
#include "chimbuko/core/util/error.hpp"

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

/* ---------------------------------------------------------------------------
 * Implementation of ExecData_t class
 * --------------------------------------------------------------------------- */
ExecData_t::ExecData_t()
  : m_pid(0), m_rid(0), m_tid(0), m_fid(0),
    m_entry(0), m_exit(0), m_runtime(0), m_exclusive(0),
    m_n_children(0), m_n_messages(0),
    m_label(0), m_gpu_correlation_id_partner(0), m_score(-1), m_references(0){}

ExecData_t::ExecData_t(const Event_t& ev) : ExecData_t()
{
    m_id = ev.id();
    m_pid = ev.pid();
    m_rid = ev.rid();
    m_tid = ev.tid();
    m_fid = ev.fid();
    m_entry = (long)ev.ts();
    m_parent = eventID::root();
}

ExecData_t::ExecData_t(const eventID &id, unsigned long pid, unsigned long rid, unsigned long tid, unsigned long fid, 
		       const std::string &func_name, long entry, long exit) : ExecData_t()
{
    m_id = id;
    m_pid = pid;
    m_rid = rid;
    m_tid = tid;
    m_fid = fid;
    m_entry = entry;
    m_parent = eventID::root();
    m_funcname = func_name;
    if(exit != -1) update_exit(exit);
}

ExecData_t::~ExecData_t() {}

void ExecData_t::deregister_reference(){ 
  if(m_references == 0) fatal_error("Attempting to deregister and event whose external reference count is 0!");
  --m_references; 
}

void ExecData_t::update_exit(unsigned long exit){
  m_exit = exit;
  m_runtime = m_exit - m_entry;
  m_exclusive += m_runtime;
}

bool ExecData_t::update_exit(const Event_t& ev)
{
  if (m_fid != ev.fid() || m_entry > (long)ev.ts())
    return false;
  update_exit(ev.ts());
  return true;
}

bool ExecData_t::add_message(const CommData_t& comm, ListEnd end) {
    if ((long)comm.ts() < m_entry || (long)comm.ts() > m_exit)
        return false;
    if(end == ListEnd::Back){
      m_messages.push_back(comm);
      m_messages.back().set_exec_key(m_id);
    }else{
      m_messages.push_front(comm);
      m_messages.front().set_exec_key(m_id);
    }

    m_n_messages++;
    return true;
}

bool ExecData_t::add_counter(const CounterData_t& counter, ListEnd end) {
    if ((long)counter.get_ts() < m_entry || (long)counter.get_ts() > m_exit)
        return false;
    if(end == ListEnd::Back){
      m_counters.push_back(counter);
      m_counters.back().set_exec_key(m_id);
    }else{
      m_counters.push_front(counter);
      m_counters.front().set_exec_key(m_id);
    }

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
    if (!(m_exclusive == other.m_exclusive)) return false;
    if (!(m_label == other.m_label)) return false;
    if (!(m_parent == other.m_parent)) return false;
    if (!(m_n_children == other.m_n_children)) return false;
    if (!(m_n_messages == other.m_n_messages)) return false;
    if (!(m_score == other.m_score)) return false;    
    return true;
}

nlohmann::json ExecData_t::get_json(bool with_message, bool with_counter) const
{
    nlohmann::json j{
        {"key", m_id.toString()},
        {"name", m_funcname},
        {"pid", m_pid}, {"tid", m_tid}, {"rid", m_rid}, {"fid", m_fid},
        {"entry", m_entry}, {"exit", m_exit},
        {"runtime", m_runtime}, {"exclusive", m_exclusive},
        {"label", m_label}, {"outlier_score", m_score},
        {"parent", m_parent.toString()},
        {"n_children", m_n_children}, {"n_messages", m_n_messages}, {"n_counters", m_counters.size() }
    };
    if (with_message)
    {
        j["messages"] = nlohmann::json::array();
        for (auto comm: m_messages)
            j["messages"].push_back(comm.get_json());
    }
    if (with_counter)
    {
        j["counters"] = nlohmann::json::array();
        for (auto count: m_counters)
            j["counters"].push_back(count.get_json());
    }
    return j;
}


const eventID & ExecData_t::get_GPU_correlationID_partner(const size_t i) const{
  if(i>=m_gpu_correlation_id_partner.size()) throw std::runtime_error("ExecData_t::get_GPU_correlationID_partner index out of range");
  return m_gpu_correlation_id_partner[i];
}

/* ---------------------------------------------------------------------------
 * Implementation of Event_t class
 * --------------------------------------------------------------------------- */
unsigned long Event_t::ts() const {
  switch (m_t) {
  case EventDataType::FUNC: return m_data[FUNC_IDX_TS];
  case EventDataType::COMM: return m_data[COMM_IDX_TS];
  case EventDataType::COUNT: return m_data[COUNTER_IDX_TS];
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

unsigned long Event_t::eid() const {
  if (m_t != EventDataType::FUNC && m_t != EventDataType::COMM) {
    std::cerr << "\n***** It is NOT func/comm event and tried to get event! *****\n";
    exit(EXIT_FAILURE);
  }
  return m_data[IDX_E];
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

unsigned long Event_t::counter_id() const{
  if (m_t != EventDataType::COUNT) {
    std::cerr << "\n***** It is NOT count event and tried to get counter id! *****\n";
    exit(EXIT_FAILURE);
  }
  return m_data[COUNTER_IDX_ID];
}

unsigned long Event_t::counter_value() const {
    if (m_t != EventDataType::COUNT) {
        std::cerr << "\n***** It is NOT count event and tried to get counter value! *****\n";
        exit(EXIT_FAILURE);
    }
    return m_data[COUNTER_IDX_VALUE];
}


int Event_t::get_data_len() const{
  int len;
  switch(type()){
  case EventDataType::FUNC:
    len = FUNC_EVENT_DIM; break;
  case EventDataType::COMM:
    len = COMM_EVENT_DIM; break;
  case EventDataType::COUNT:
    len = COUNTER_EVENT_DIM; break;
  default:
    throw std::runtime_error("Invalid EventDataType");
  }
  return len;
}

bool Event_t::operator==(const Event_t &r) const{
  if(r.m_t != m_t || r.m_id != m_id || r.m_idx != m_idx) return false;
  int len = get_data_len();
  for(int i=0;i<len;i++) if(m_data[i] != r.m_data[i]) return false;
  return true;
}


bool chimbuko::modules::performance_analysis::operator<(const Event_t& lhs, const Event_t& rhs) {
    if (lhs.ts() == rhs.ts()) return lhs.idx() < rhs.idx();
    return lhs.ts() < rhs.ts();
}

bool chimbuko::modules::performance_analysis::operator>(const Event_t& lhs, const Event_t& rhs) {
    if (lhs.ts() == rhs.ts()) return lhs.idx() > rhs.idx();
    return lhs.ts() > rhs.ts();
}

nlohmann::json Event_t::get_json() const
{
  if (!valid())
    return {};

  nlohmann::json j{
                   {"id", id().toString()}, {"idx", idx()}, {"type", strtype()},
		   {"pid", pid()}, {"rid", rid()}, {"tid", tid()}, {"ts", ts()}
  };
  if (m_t == EventDataType::FUNC) {
    j["eid"] = eid();
    j["fid"] = fid();
  }
  if (m_t == EventDataType::COMM) {
    j["eid"] = eid();
    j["tag"] = tag();
    j["partner"] = partner();
    j["bytes"] = bytes();
  }
  if (m_t == EventDataType::COUNT) {
    j["counter_id"] = counter_id();
    j["value"] = counter_value();
  }

  return j;
}

/* ---------------------------------------------------------------------------
 * Implementation of CommData_t class
 * --------------------------------------------------------------------------- */
CommData_t::CommData_t()
{

}

CommData_t::CommData_t(const Event_t& ev, const std::string &commType)
: m_commType(commType)
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

CommData_t::CommData_t(unsigned long pid, unsigned long rid, unsigned long tid,
		       unsigned long partner, unsigned long bytes, unsigned long tag,
		       unsigned long timestamp, const std::string &commType)
  : m_commType(commType), m_pid(pid), m_rid(rid), m_tid(tid), m_bytes(bytes), m_tag(tag), m_ts(timestamp){
  if (m_commType.compare("SEND") == 0) {
    m_src = m_rid;
    m_tar = partner;
  } else if (m_commType.compare("RECV") == 0){
    m_src = partner;
    m_tar = m_rid;
  }
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
    if (!(m_execkey == other.m_execkey)) return false;
    return true;
}

nlohmann::json CommData_t::get_json() const
{
    return {
        {"type", m_commType},
        {"pid", m_pid}, {"rid", m_rid}, {"tid", m_tid},
        {"src", m_src}, {"tar", m_tar},
        {"bytes", m_bytes}, {"tag", m_tag},
        {"timestamp", m_ts},
        {"execdata_key", m_execkey.toString()}
    };
}

MetaData_t::MetaData_t(unsigned long pid, unsigned long rid, unsigned long tid, const std::string &descr, const std::string &value):
  m_pid(pid), m_rid(rid), m_tid(tid), m_descr(descr), m_value(value){}

nlohmann::json MetaData_t::get_json() const{
  return {
    {"pid", m_pid},
      {"rid",m_rid},
	{"tid",m_tid},
	  {"descr",m_descr},
	    {"value",m_value}
  };
}


CounterData_t::CounterData_t():
  m_countername("none"),
  m_pid(0),
  m_rid(0),
  m_tid(0),
  m_cid(-1),
  m_value(-1),
  m_ts(-1){}

CounterData_t::CounterData_t(const Event_t& ev, const std::string &counter_name):
  m_countername(counter_name),
  m_pid(ev.pid()),
  m_rid(ev.rid()),
  m_tid(ev.tid()),
  m_cid(ev.counter_id()),
  m_value(ev.counter_value()),
  m_ts(ev.ts()){}


CounterData_t::CounterData_t(unsigned long pid, unsigned long rid, unsigned long tid,
			     unsigned long counter_id, const std::string &counter_name, 
			     unsigned long counter_value,
			     unsigned long timestamp):
  m_countername(counter_name),
  m_pid(pid),
  m_rid(rid),
  m_tid(tid),
  m_cid(counter_id),
  m_value(counter_value),
  m_ts(timestamp){}



nlohmann::json CounterData_t::get_json() const{
  return {
	  {"pid",m_pid},
	  {"rid",m_rid},
	  {"tid",m_tid},
	  {"ts",m_ts},
	  {"counter_idx", m_cid },
	  {"counter_name",m_countername},
	  {"counter_value",m_value}
  };
}
