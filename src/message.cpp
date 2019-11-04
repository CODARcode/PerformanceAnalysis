#include "chimbuko/message.hpp"
#include <sstream>

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
    if (include_head)
    {
        nlohmann::json j = nlohmann::json::parse(msg);
        m_head.set_header(j["Header"]);
        m_buf = j["Buffer"].dump();
    }
    else
    {
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
    nlohmann::json j;
    j["Header"] = m_head.get_json();
    try
    {
        j["Buffer"] = nlohmann::json::parse(m_buf);
    }
    catch(const std::exception& e)
    {
        j["Buffer"] = m_buf;
    }
    
    return j.dump();
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
