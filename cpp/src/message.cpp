#include "chimbuko/message.hpp"
#include <sstream>

using namespace chimbuko;

// ---------------------------------------------------------------------------
// Implementation of Message::Header class
// ---------------------------------------------------------------------------
std::ostream& chimbuko::operator<<(std::ostream& os, const Message::Header& h)
{
    os.write((const char*)h.m_h, Message::Header::headerSize);
    return os;
}

std::istream& chimbuko::operator>>(std::istream& is, Message::Header& h)
{
    is.read((char*)h.m_h, Message::Header::headerSize);
    return is;
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
        std::stringstream ss(
            msg,
            std::stringstream::out | std::stringstream::in | std::stringstream::binary
        );
        ss >> m_head;
        m_buf = msg.substr(Message::Header::headerSize);
    }
    else
    {
        m_buf = std::string(msg);
        m_head.size() = m_buf.size();            
    }        
}

void Message::set_msg(int cmd)
{
    set_msg(std::to_string(cmd), false);
}

std::string Message::data() const
{
    std::stringstream oss(std::stringstream::out | std::stringstream::binary);
    oss << m_head;
    oss << m_buf;
    return oss.str();
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