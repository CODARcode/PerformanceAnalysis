#include "chimbuko/core/message.hpp"
#include <sstream>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/string.hpp>


using namespace chimbuko;

std::string chimbuko::toString(const MessageType m){
#define KSTR(A) case A: return #A

  switch(m){
    KSTR(REQ_ADD);
    KSTR(REQ_GET);
    KSTR(REQ_CMD);
    KSTR(REQ_QUIT);
    KSTR(REQ_ECHO);
    KSTR(REP_ADD);
    KSTR(REP_GET);
    KSTR(REP_CMD);
    KSTR(REP_QUIT);
    KSTR(REP_ECHO);
    default: return "UNKNOWN";
  }
#undef KSTR
}

std::string chimbuko::toString(const MessageKind m){
#define KSTR(A) case A: return #A

  switch(m){
    KSTR(DEFAULT);
    KSTR(CMD);
    KSTR(PARAMETERS);
    KSTR(ANOMALY_STATS);
    KSTR(COUNTER_STATS);
    KSTR(FUNCTION_INDEX);
    KSTR(ANOMALY_METRICS);
    KSTR(AD_PS_COMBINED_STATS);
    default: return "UNKNOWN";
  }
#undef KSTR
}

std::string chimbuko::toString(const MessageCmd m){
#define KSTR(A) case A: return #A

  switch(m){
    KSTR(QUIT);
    KSTR(ECHO);
    default: return "UNKNOWN";
  }
#undef KSTR
}

// ---------------------------------------------------------------------------
// Implementation of Message::Header class
// ---------------------------------------------------------------------------
nlohmann::json Message::Header::get_json() const 
{
    return {
        {"src", src()},
        {"dst", dst()},
        {"type", type()},
        {"kind", kind()},
        {"size", size()},
        {"frame", frame()}
    };
}

void Message::Header::set_header(const nlohmann::json& j)
{
    src() = j["src"];
    dst() = j["dst"];
    type() = j["type"];
    kind() = j["kind"];
    size() = j["size"];
    frame() = j["frame"];
}

void Message::Header::set_header(const std::string& s)
{
    nlohmann::json j = nlohmann::json::parse(s);
    set_header(j);
}

// ---------------------------------------------------------------------------
// Implementation of Message class
// ---------------------------------------------------------------------------
void Message::set_info(int src, int dst, int type, int kind, int frame, int size)
{
    m_head.src() = src;
    m_head.dst() = dst;
    m_head.type() = (int)type;
    m_head.kind() = (int)kind;
    m_head.size() = (int)size;
    m_head.frame() = (int)frame;
}

void Message::deserializeMessage(const std::string& from){
  std::stringstream ss; ss << from;
  cereal::PortableBinaryInputArchive rd(ss);
  rd(*this);
}

void Message::setContent(const std::string& msg){
  m_buf = msg;
  m_head.size() = m_buf.size();
}

std::string Message::serializeMessage() const 
{
  std::stringstream ss;
  {
    cereal::PortableBinaryOutputArchive wr(ss);
    wr(*this);
  }
  return ss.str();
}


Message Message::createReply() const
{
    Message reply;
    reply.m_head.src() = m_head.dst();
    reply.m_head.dst() = m_head.src();
    reply.m_head.type() = m_head.type() * 10;
    reply.m_head.kind() = m_head.kind();
    reply.m_head.size() = m_head.size();
    reply.m_head.frame() = m_head.frame();
    return reply;
}

std::string Message::kind_str() const {
  return toString((MessageKind)m_head.kind());
}
