#pragma once
#include <chimbuko_config.h>
#include <string>
#include <iostream>
#include <nlohmann/json.hpp>

namespace chimbuko {

/**
  * @brief Enum of the message "type" or action
  */
enum MessageType {
    REQ_ADD  =  1,
    REQ_GET  =  2,
    REQ_CMD  =  3,
    REQ_QUIT =  4,
    REQ_ECHO =  5,
    REP_ADD  = 10,
    REP_GET  = 20,
    REP_CMD  = 30,
    REP_QUIT = 40,
    REP_ECHO = 50
};

std::string toString(const MessageType m);

/**
 * @brief Enum of the message "kind" describing the context of the action
 */
enum MessageKind {
    DEFAULT = 0,
    CMD     = 1,
    PARAMETERS    = 2,
    ANOMALY_STATS = 3,
    COUNTER_STATS = 4,
    FUNCTION_INDEX = 5,
    ANOMALY_METRICS = 6,
    AD_PS_COMBINED_STATS = 7
};

std::string toString(const MessageKind m);


enum MessageCmd {
    QUIT = 0,
    ECHO = 1
};

std::string toString(const MessageCmd m);


/**
 * @brief A class containing a message and header that can be serialized in JSON form for communication
 */
class Message {
public:
    class Header {
    public:
        /**
         * @brief header size in bytes 
         * 
         */
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

        nlohmann::json get_json() const;
        void set_header(const nlohmann::json& j);
        void set_header(const std::string& s);

        /**
	 * @brief Serialize using cereal
	 */
        template<class Archive>
	void serialize(Archive & archive){
	  archive(m_h);
	}
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
     * @brief Set the message contents. 
     *
     * If 'include_head' is true, the string 'msg' will be interpreted as a serialized message and it will be unpacked into this object (use to parse received messages)
     * If 'include_head' is false, the message contents will be set to 'msg' and the header will be set to contain the length of the string as its size entry (use to generate new messages to send)
     */
    void set_msg(const std::string& msg, bool include_head=false); 
  
    /**
     * @brief Set the message contents to an integer; equivalent to set_msg(int_as_string, false)
     */
    void set_msg(int cmd);

    /**
     * @brief Return the message contents as a string
     */
    const std::string& buf() const { return m_buf; }; 
    /**
     * @brief Return the message in serialized form
     */
    std::string data() const;

    /**
     * @brief Get the origin rank
     */
    int src() const { return m_head.src(); }

    /**
     * @brief Get the destination rank
     */
    int dst() const { return m_head.dst(); }

    /**
     * @brief Get the message type
     */
    int type() const { return m_head.type(); }

    /**
     * @brief Get the message kind
     */
    int kind() const { return m_head.kind(); }

    /**
     * @brief Get the message kind in string form
     */
    std::string kind_str() const;

   /**
    * @brief Get the message size in bytes
    */
    int size() const { return m_head.size(); }

    /**
     * @brief Get the message io frame (step)
     */
    int frame() const { return m_head.frame(); } 

    /**
     * @brief clear data buffer
     * 
     */
    void clear() { m_buf.clear(); }

    /**
     * @brief Create a message reply with the source, destination, type and kind appropriately filled
     */
    Message createReply() const;

    /**
     * @brief Write the message to the output stream in JSON form
     */
    void show(std::ostream& os) const {
        os << m_head.get_json().dump();
    }

    /**
     * @brief Serialize using cereal
     */
    template<class Archive>
    void serialize(Archive & archive){
      archive(m_head, m_buf);
    }

private:
    Header m_head; /**< Message header*/
    std::string m_buf; /**< Message content*/
};

} // end of namespace chimbuko
