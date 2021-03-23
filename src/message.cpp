#include "chimbuko/message.hpp"
#include <sstream>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/string.hpp>


using namespace chimbuko;

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

void Message::set_msg(const std::string& msg, bool include_head)
{
  if(include_head){
    std::stringstream ss; ss << msg;
    cereal::PortableBinaryInputArchive rd(ss);
    rd(*this);
  }else{
    m_buf = msg;
    m_head.size() = m_buf.size();
  }
}

void Message::set_msg(int cmd)
{
    set_msg(std::to_string(cmd));
}

std::string Message::data() const 
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
