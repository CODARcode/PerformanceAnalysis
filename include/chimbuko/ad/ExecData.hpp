#pragma once
#include <chimbuko/ad/ADDefine.hpp>
#include <chimbuko/ad/utils.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <nlohmann/json.hpp>

namespace chimbuko {

/**
 * @brief class to provide easy access to raw performance event vector
 *
 * The data are passed in via ADIOS2 and stored internally in a compressed format in the form of an integer array, blocks of which are associated with
 * particular events. Each block has a certain number of entries associated with it that relate to information such as program, comm and thread index,
 * timestamp as well as detailed event information. The mappings are set out in ADDefine.hpp.
 *
 * This class wraps the event data blocks allowing for retrieval of event information through explicit function calls.
 * It works for all event types: function, comm and counter
 */
class Event_t {
public:
    /**
     * @brief Construct a new Event_t object
     *
     * @param data pointer to raw performance event vector
     * @param t event type
     * @param idx event index
     * @param id event id
     */
   Event_t(const unsigned long * data, EventDataType t, size_t idx, const eventID &id = eventID())
        : m_data(data), m_t(t), m_id(id), m_idx(idx) {}
    /**
     * @brief Destroy the Event_t object
     *
     */
    ~Event_t() {}

    /**
     * @brief check if the raw data pointer is valid
     */
    bool valid() const {
        return m_data != nullptr;
    }
    /**
     * @brief return event id
     */
    const eventID &id() const { return m_id; }
    /**
     * @brief return event index, typically the index of the event in the input array for the timestep on which it was spawned
     */
    size_t idx() const { return m_idx; }
    /**
     * @brief return program id
     */
    unsigned long pid() const { return m_data[IDX_P]; }
    /**
     * @brief return rank id
     */
    unsigned long rid() const { return m_data[IDX_R]; }
    /**
     * @brief return thread id
     */
    unsigned long tid() const { return m_data[IDX_T]; }
    /**
     * @brief return event type id (FUNC/COMM only). Eg for FUNC events is is ENTRY/EXIT
     */
    unsigned long eid() const;
    /**
     * @brief return timestamp of this event
     */
    unsigned long ts() const;

    /**
     * @brief return event type
     */
    EventDataType type() const { return m_t; }
    /**
     * @brief return string event type
     */
    std::string strtype() const;

    /**
     * @brief return function (timer) id   (FUNC event only)
     */
    unsigned long fid() const;

    /**
     * @brief return communication tag id  (COMM event only)
     */
    unsigned long tag() const;
    /**
     * @brief return communication partner id (COMM event only)
     */
    unsigned long partner() const;
    /**
     * @brief return communication data size (in bytes)  (COMM event only)
     */
    unsigned long bytes() const;

    /**
     * @brief return counter id   (COUNT event only)
     */
    unsigned long counter_id() const;
    /**
     * @brief return the value of the counter  (COUNT event only)
     */
    unsigned long counter_value() const;

    /**
     * @brief compare two events
     */
    friend bool operator<(const Event_t& lhs, const Event_t& rhs);
    /**
     * @brief compare two events
     */
    friend bool operator>(const Event_t& lhs, const Event_t& rhs);

    /**
     * @brief Equivalence operation
     *
     * Note the underlying array pointers can be different providing the values are identical
     */
    bool operator==(const Event_t &r) const;

    /**
     * @brief Get the json object of this event object
     */
    nlohmann::json get_json() const;

    /**
     * @brief Return the pointer to the underlying data
     */
    const unsigned long* get_ptr() const{ return m_data; }

    /**
     * @brief Get the length of the underlying array
     */
    int get_data_len() const;

private:
    const unsigned long * m_data;   /**< pointer to raw performance trace data vector */
    EventDataType m_t;              /**< event type */
    eventID m_id;               /**< event id */
    size_t m_idx;                   /**< event index */
};

/**
 * @brief compare two events
 */
bool operator<(const Event_t& lhs, const Event_t& rhs);
/**
 * @brief compare two events
 */
bool operator>(const Event_t& lhs, const Event_t& rhs);


/**
 * @brief wrapper for communication event
 *
 */
class CommData_t {
public:
    /**
     * @brief Construct a new CommData_t object
     *
     */
    CommData_t();
    /**
     * @brief Construct a new CommData_t object
     *
     * @param ev constant reference to a Event_t object
     * @param commType communication type (e.g. SEND/RECV)
     */
    CommData_t(const Event_t& ev, const std::string &commType);
    /**
     * @brief Destroy the CommData_t object
     *
     */
    ~CommData_t() {};

    /**
     * @brief return communication type
     */
    std::string type() const { return m_commType; }
    /**
     * @brief return timestamp
     */
    unsigned long ts() const { return m_ts; }
    /**
     * @brief return source process id of this communication event
     */
    unsigned long src() const { return m_src; }
    /**
     * @brief return target (or destination) process id of this communication event
     */
    unsigned long tar() const { return m_tar; }

    /**
     * @brief Get the integer tag associated with this comm event
     */
    unsigned long tag() const{ return m_tag; }

    /**
     * @brief Set the execution key id (i.e. where this communication event occurs). This is equal to the "id" string associated with a parent ExecData_t object
     *
     * @param key execution id
     */
    void set_exec_key(const eventID &key) { m_execkey = key; }

    /**
     * @brief Get the execution key id. This is equal to the "id" string associated with a parent ExecData_t object
     */
    const eventID & get_exec_key() const { return m_execkey; }

    /**
     * @brief compare two communication data
     *
     * @param other
     * @return true if other is same
     * @return false if other is different
     */
    bool is_same(const CommData_t& other) const;
    /**
     * @brief Get the json object of this communication data
     */
    nlohmann::json get_json() const;

private:
    std::string m_commType;             /**< communication type */
    unsigned long
        m_pid,                          /**< program id */
        m_rid,                          /**< rank id */
        m_tid;                          /**< thread id */
    unsigned long
        m_src,                          /**< source process id */
        m_tar;                          /**< target process id */
    unsigned long
        m_bytes,                        /**< communication data size in bytes */
        m_tag;                          /**< communication tag */
    unsigned long m_ts;                 /**< communication timestamp */
    eventID m_execkey;              /**< execution key (or id) where this communication event occurs */
};



/**
 * @brief wrapper for counter event
 *
 */
  class CounterData_t {
  public:
    /**
     * @brief Construct a new CounterData_t object
     *
     */
    CounterData_t();
    /**
     * @brief Construct a new CounterData_t object
     *
     * @param ev constant reference to a Event_t object
     * @param commType communication type (e.g. SEND/RECV)
     */
    CounterData_t(const Event_t& ev, const std::string &counter_name);

    /**
     * @brief Get the json object of this communication data
     */
    nlohmann::json get_json() const;

    /**
     * @brief return program id
     */
    unsigned long get_pid() const { return m_pid; }
    /**
     * @brief return rank id
     */
    unsigned long get_rid() const { return m_rid; }
    /**
     * @brief return thread id
     */
    unsigned long get_tid() const { return m_tid; }

    /**
     * @brief Return the name of the counter
     */
    const std::string & get_countername() const{ return m_countername; }

    /**
     * @brief Return the value of the counter
     */
    unsigned long get_value() const { return m_value; }

    /**
     * @brief Return the counter timestamp
     */
    unsigned long get_ts() const { return m_ts; }

    /**
     * @brief Return the index of the counter
     */
    unsigned long get_counterid() const{ return m_cid; }

    /**
     * @brief Set the execution key id (i.e. where this counter event occurs). This is equal to the "id" string associated with a parent ExecData_t object
     *
     * @param key execution id
     */
    void set_exec_key(const eventID &key) { m_execkey = key; }

    /**
     * @brief Get the execution key id. This is equal to the "id" string associated with a parent ExecData_t object
     */
    const eventID & get_exec_key() const { return m_execkey; }

    private:
    std::string m_countername;             /**< counter name */
    unsigned long
    m_pid,                          /**< program id */
      m_rid,                          /**< rank id */
      m_tid,                          /**< thread id */
      m_cid,                          /**< counted id */
      m_value,                        /**< counter value */
      m_ts;                           /**< counter timestamp */
     eventID m_execkey;              /**< execution key (or id) where this counter event occurs */
  };




/**
 * @brief A pair of function (timer) events, ENTRY and EXIT.
 *
 */
class ExecData_t {

public:
    /**
     * @brief Construct a new ExecData_t object
     *
     */
    ExecData_t();
    /**
     * @brief Construct a new ExecData_t object
     *
     * @param ev constant reference to a Event_t object
     */
    ExecData_t(const Event_t& ev);
    /**
     * @brief Destroy the ExecData_t object
     *
     */
    ~ExecData_t();

    /**
     * @brief Get the id of this execution data
     */
    const eventID &get_id() const { return m_id; }
    /**
     * @brief Get the function name of this execution data
     */
    const std::string &get_funcname() const { return m_funcname; }
    /**
     * @brief Get the program id of this execution data
     */
    unsigned long get_pid() const { return m_pid; }
    /**
     * @brief Get the thread id of this execution data
     */
    unsigned long get_tid() const { return m_tid; }
    /**
     * @brief Get the rank id of this execution data
     */
    unsigned long get_rid() const { return m_rid; }
    /**
     * @brief Get the function id of this execution data
     */
    unsigned long get_fid() const { return m_fid; }
    /**
     * @brief Get the entry time of this execution data
     */
    long get_entry() const { return m_entry; }
    /**
     * @brief Get the exit time of this execution data
     */
    long get_exit() const { return m_exit; }
    /**
     * @brief Get the (inclusive) running time of this execution data
     */
    long get_runtime() const { return m_runtime; }
    /**
     * @brief Get the (inclusive) running time of this execution data
     */
    long get_inclusive() const { return m_runtime; }
    /**
     * @brief Get the exclusive running ime of this execution data
     */
    long get_exclusive() const { return m_exclusive; }
    /**
     * @brief Get the label of this execution data
     *
     * @return int 1 of normal and -1 if anomaly. Returns 0 if no label has been assigned.
     */
    int get_label() const { return m_label; }
    /**
     * @brief Get the parent function id of this execution data
     */
    const eventID & get_parent() const { return m_parent; }
    /**
     * @brief Get a list of communication data occured in this execution data
     */
    const std::deque<CommData_t>& get_messages() const{ return m_messages; }

    /**
     * @brief Get a list of counter events that occured in this execution data
     */
    const std::deque<CounterData_t>& get_counters() const{ return m_counters; }

    /**
     * @brief Get the number of communication events
     */
    unsigned long get_n_message() const { return m_n_messages; }
    /**
     * @brief Get the number of childrent functions
     */
    unsigned long get_n_children() const { return m_n_children; }

    /**
     * @brief Get the number of counter
     */
    unsigned long get_n_counter() const { return m_counters.size(); }

    /**
     * @brief Set the label
     *
     * @param label 1 for normal, -1 for anomaly.
     */
    void set_label(int label) { m_label = label; }
    /**
     * @brief Set the parent function of this execution
     *
     * @param parent the parent execution id
     */
    void set_parent(const eventID &parent) { m_parent = parent; }
    /**
     * @brief Set the function name of this execution
     *
     * @param funcname function name
     */
    void set_funcname(std::string funcname) { m_funcname = funcname; }

    /**
     * @brief update exit event of this execution
     *
     * @param ev exit event
     * @return true no errors
     * @return false incorrect exit event
     */
    bool update_exit(const Event_t& ev);
    /**
     * @brief update exclusive running time
     *
     * @param t running time of a child function
     */
    void update_exclusive(long t) { m_exclusive -= t; }
    /**
     * @brief increase the number of child function by 1
     *
     */
    void inc_n_children() { m_n_children++; }
    /**
     * @brief add communication data to one end of the message queue
     *
     * @param comm communication event occured in this execution
     * @param end add to which end of the deque
     * @return true no errors
     * @return false invalid communication event
     */
    bool add_message(const CommData_t& comm, ListEnd end = ListEnd::Back);

    /**
     * @brief add counter data
     *
     * @param counter counter event occurred in this execution
     * @param end add to which end of the deque
     * @return true no errors
     * @return false invalid communication event
     */
    bool add_counter(const CounterData_t& count, ListEnd end = ListEnd::Back);


    /**
     * @brief compare with other execution
     *
     * @param other other execution data
     * @return true if they are same
     * @return false if they are different
     */
    bool is_same(const ExecData_t& other) const;

    /**
     * @brief Get the json object of this execution data
     *
     * @param with_message if true, include all message (communication) information
     * @param with_counter if true, include all counter information
     * @return nlohmann::json json object
     */
    nlohmann::json get_json(bool with_message=false, bool with_counter=false) const;

    /**
     * @brief Determine whether the event can be deleted by the garbage collection at the end of the io step
     */
    bool can_delete() const{ return m_can_delete;}

    /**
     * @brief Set whether the event can be deleted by the garbage collection at the end of the io step (default true)
     */
    void can_delete(const bool v){ m_can_delete = v; }


    /**
     * @brief Set the partner event linked by a GPU correlation ID
     */
    void set_GPU_correlationID_partner(const eventID &event_id){ m_gpu_correlation_id_partner.push_back(event_id); }

    /**
     * @brief Return true if this event has been matched to a partner event by a GPU correlation ID
     */
    bool has_GPU_correlationID_partner() const{ return m_gpu_correlation_id_partner.size() > 0; }

    /**
     * @brief Get the number of events linked by GPU correlation ID
     */
    size_t n_GPU_correlationID_partner() const{ return m_gpu_correlation_id_partner.size(); }

    /**
     * @brief Get the partner event linked by a GPU correlation ID
     * @param i index in array of partners
     *
     * Throws error if index out of range
     */
    const eventID & get_GPU_correlationID_partner(const size_t i) const;

private:
    eventID m_id;                        /**< execution id */
    std::string m_funcname;              /**< function name */
    unsigned long
        m_pid,                           /**< program id */
        m_tid,                           /**< thread id */
        m_rid,                           /**< rank id */
        m_fid;                           /**< function id */
    long
        m_entry,                         /**< entry time */
        m_exit,                          /**< exit time */
        m_runtime,                       /**< inclusive running time (i.e. including time of child calls) */
        m_exclusive;                     /**< exclusive running time (i.e. excluding time of child calls) */
    int m_label;                         /**< 1 for normal, -1 for abnormal execution */
    eventID m_parent;                    /**< parent execution */
    unsigned long m_n_children;          /**< the number of childrent executions */
    unsigned long m_n_messages;          /**< the number of messages */
    std::deque<CommData_t> m_messages;  /**< a vector of all messages */
    std::deque<CounterData_t> m_counters; /**< a vector of all counters */
    bool m_can_delete; /**< Flag indicating that the event is deletable by the garbage collection */
    std::vector<eventID> m_gpu_correlation_id_partner;  /**< The event ids partner events linked by a correlation ID, either the launching CPU event or the GPU kernel event */
};



/**
 * @brief wrapper for metadata entries
 *
 */
class MetaData_t {
public:
  /**
   * @brief Construct an instance will full set of parameters
   * @param pid Program index
   * @param rid Rank
   * @param tid Thread
   * @param descr Key
   * @param descr Value
   */
  MetaData_t(unsigned long pid, unsigned long rid, unsigned long tid,
	     const std::string &descr, const std::string &value);

  /**
   * @brief Get the origin program index
   */
  unsigned long get_pid() const{ return m_pid; }

  /**
   * @brief Get the origin global comm rank
   */
  unsigned long get_rid() const{ return m_rid; }

  /**
   * @brief Get the origin thread index
   */
  unsigned long get_tid() const{ return m_tid; }

  /**
   * @brief Get the metadata description
   */
  const std::string & get_descr() const{ return m_descr; }

  /**
   * @brief Get the metadata value
   */
  const std::string & get_value() const{ return m_value; }

  /**
   * @brief Get the json object of this metadata
   *
   * @return nlohmann::json json object
   */
  nlohmann::json get_json() const;
private:
  unsigned long m_pid; /**< Program index */
  unsigned long m_rid; /**< Global comm rank */
  unsigned long m_tid; /**< Thread idx */
  std::string m_descr; /**< Metadata description */
  std::string m_value; /**< Metadata value */
};



}; // end of AD namespace
