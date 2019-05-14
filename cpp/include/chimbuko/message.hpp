#pragma once
#include <string>
#include <iostream>

namespace chimbuko {

enum MessageType {
    REQ_ADD  =  1,
    REQ_GET  =  2,
    REQ_CMD  =  3,
    REQ_QUIT =  4,
    REP_ADD  = 10,
    REP_GET  = 20,
    REP_CMD  = 30,
    REP_QUIT = 40
};

enum MessageKind {
    DEFAULT = 0,
    CMD     = 1,
    SSTD    = 2
};

enum MessageCmd {
    QUIT = 0
};

class Message {
public:
    class Header {
    public:
        /**
         * @brief header size in bytes 
         * 
         */
        static const size_t headerSize = 8 * sizeof(int);

        Header() 
        {
            for (int i = 0; i < 8; i++)
                m_h[i] = 0;
        }
        
        /**
         * @brief source rank 
         * 
         * @return int& reference to the source rank
         */
        int& src() { return m_h[0]; }
        int src() const { return m_h[0]; }
        /**
         * @brief desination rank 
         * 
         * @return int& reference to the destination rank
         */
        int& dst() { return m_h[1]; }
        int dst() const { return m_h[1]; }
        /**
         * @brief message type
         * 
         * @return int& reference to the message type
         */
        int& type() { return m_h[2]; }
        int type() const { return m_h[2]; }
        /**
         * @brief message kind
         * 
         * @return int& reference to the message kind
         */
        int& kind() { return m_h[3]; }
        int kind() const { return m_h[3]; }
        /**
         * @brief message size
         * 
         * @return int& reference to the message size
         */
        int& size() { return m_h[4]; }
        int size() const { return m_h[4]; }
        /**
         * @brief message frame index
         * 
         * @return int& reference to the message frame index
         */
        int& frame() { return m_h[5]; }
        int frame() const { return m_h[5]; }

        /**
         * @brief write header to the binary stream
         * 
         * @param os binary output stream
         * @param h message header object
         * @return std::ostream& binary output stream
         */
        friend std::ostream& operator<<(std::ostream& os, const Header& h);
        /**
         * @brief read header from the binary stream
         * 
         * @param is binary input stream
         * @param h message header object
         * @return std::istream& binary input stream
         */
        friend std::istream& operator>>(std::istream& is, Header& h);
    private:
        /**
         * @brief header information
         *  
         *  0: src rank
         *  1: dst rank
         *  2: message type
         *  3: message kind
         *  4: message size (except header) in bytes
         *  5: frame index (or step index)
         *  6: reserved
         *  7: reserved
         */
        int m_h[8];
    };

public:
    /**
     * @brief Construct a new Message object
     * 
     */
    Message() {};
    /**
     * @brief Destroy the Message object
     * 
     */
    ~Message() {};

    /**
     * @brief Set the message information (header)
     * 
     * @param src source rank
     * @param dst destination rank
     * @param type message type
     * @param kind message kind
     * @param frame frame index
     * @param size message size
     */
    void set_info(int src, int dst, int type, int kind, int frame = 0, int size = 0);

    /**
     * @brief Set the message buffer
     * 
     * @param msg binary message string
     * @param include_head if true, the msg also contains header information.
     */
    void set_msg(const std::string& msg, bool include_head=false); 
    void set_msg(int cmd);

    /**
     * @brief message size (data buffer size + header size)
     * 
     * @return size_t message size
     */
    size_t count() const { return Header::headerSize + m_buf.size(); }

    /**
     * @brief message buffer (header + data buffer)
     * 
     * @return std::string message buffer
     */
    std::string data() const; 

    std::string data_buffer() const { return m_buf; }

    int src() const { return m_head.src(); }

    int dst() const { return m_head.dst(); }

    int type() const { return m_head.type(); }

    int kind() const { return m_head.kind(); }

    int size() const { return m_head.size(); }

    int frame() const { return m_head.frame(); } 

    /**
     * @brief clear data buffer
     * 
     */
    void clear() { m_buf.clear(); }

    Message createReply() const;

    void show(std::ostream& os) const {
        os << "\nsrc: " << m_head.src() 
           << "\ndst: " << m_head.dst()
           << "\ntype: " << m_head.type()
           << "\nkind: " << m_head.kind()
           << "\nsize: " << m_head.size()
           << "\nframe: " << m_head.frame()
           << std::endl;
    }

private:
    Header m_head;
    std::string m_buf;
};

std::ostream& operator<<(std::ostream& os, const Message::Header& h);
std::istream& operator>>(std::istream& is, Message::Header& h);

} // end of namespace chimbuko
