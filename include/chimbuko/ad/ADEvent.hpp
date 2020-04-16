#pragma once
#include "chimbuko/ad/ADDefine.hpp"
#include "chimbuko/ad/ExecData.hpp"
#include <string>
#include <vector>
#include <list>
#include <stack>
#include <unordered_map>

namespace chimbuko {    
  /**
   * @brief a stack of CommData_t
   * 
   */
  typedef std::stack<CommData_t> CommStack_t;

  /**
   * @brief map of process, rank, thread -> Commstack_t
   */
  DEF_MAP3UL(CommStackMap, CommStack_t); 
  
  /**
   * @brief list of function calls (ExecData_t) in entry time order
   * 
   */
  typedef std::list<ExecData_t> CallList_t;

  /**
   * @brief iterator of CallList_t
   * 
   */
  typedef CallList_t::iterator  CallListIterator_t;

  /**
   * @brief map of process, rank, thread -> CallList_t
   */
  DEF_MAP3UL(CallListMap, CallList_t);
  

  /**
   * @brief function call stack
   */
  typedef std::stack<CallListIterator_t> CallStack_t;

  /**
   * @bried map of process, rank, thread -> CallListIterator_t
   */
  DEF_MAP3UL(CallStackMap, CallStack_t);


  /**
   * @brief hash map of a collection of ExecData_t per function
   * 
   * key is function id and value is a vector of CallListIterator_t (i.e. ExecData_t)
   * 
   */
  typedef std::unordered_map<unsigned long, std::vector<CallListIterator_t>> ExecDataMap_t;

  /**
   * @brief Event manager
   * 
   */
  class ADEvent {

  public:
    /**
     * @brief Construct a new ADEvent object
     * 
     * @param verbose true to print out detailed information (useful for debug)
     */
    ADEvent(bool verbose=false);
    /**
     * @brief Destroy the ADEvent object
     * 
     */
    ~ADEvent();

    /**
     * @brief copy a pointer that is externally defined even type object 
     * 
     * @param m event type object (hash map)
     */
    void linkEventType(const std::unordered_map<int, std::string>* m) { m_eventType = m; }

    /**
     * @brief copy a pointer that is externally defined function map object
     * 
     * @param m function map object
     */
    void linkFuncMap(const std::unordered_map<int, std::string>* m) { m_funcMap = m; }

    /**
     * @brief Get the Func Map object
     * 
     * @return const std::unordered_map<int, std::string>* pointer to function map object
     */
    const std::unordered_map<int, std::string>* getFuncMap() const { return m_funcMap; }

    /**
     * @brief Get the Event Type object
     * 
     * @return const std::unordered_map<int, std::string>* pointer to event type object
     */
    const std::unordered_map<int, std::string>* getEventType() const { return m_eventType; }

    /**
     * @brief Get the Exec Data Map object
     * 
     * @return const ExecDataMap_t* pointer to ExecDataMap_t object
     */
    const ExecDataMap_t* getExecDataMap() const { return &m_execDataMap; }

    /**
     * @brief Get the Call List Map object
     * 
     * @return const CallListMap_p_t* pointer to CallListMap_p_t object
     */
    const CallListMap_p_t * getCallListMap() const { return &m_callList; }
    /**
     * @brief Get the Call List Map object
     * 
     * @return CallListMap_p_t& pointer to CallListMap_p_t object
     */
    CallListMap_p_t& getCallListMap() { return m_callList; }

    /**
     * @brief Get an iterator to an ExecData_t instance with given event index string
     */
    CallListIterator_t getCallData(const std::string &event_id) const;     
    
    /**
     * @brief clear 
     * 
     */
    void clear();

    /**
     * @brief add an event
     * 
     * @param event function or communication event
     * @return EventError event error code
     */
    EventError addEvent(const Event_t& event);
    /**
     * @brief add a function event
     * 
     * @param event function event
     * @return EventError event error code
     */
    EventError addFunc(const Event_t& event);
    /**
     * @brief add a communication event
     * 
     * @param event communication event
     * @return EventError event error code
     */
    EventError addComm(const Event_t& event);

    /**
     * @brief trim out all function calls that are completed (i.e. a pair of ENTRY and EXIT events are observed)
     * 
     * @return CallListMap_p_t* trimed function calls
     */
    CallListMap_p_t* trimCallList();


    /**
     * @brief show current call stack tree status
     * 
     * @param verbose true to see all details
     */
    void show_status(bool verbose=false) const;

  private:
    /**
     * @brief pointer to map of function index to function name
     * 
     */
    const std::unordered_map<int, std::string> *m_funcMap;
    /**
     * @brief pointer to map of event index to event type string
     * 
     */
    const std::unordered_map<int, std::string> *m_eventType;

    /**
     * @brief communication event stack. Once a function call has exited, all comms events are associated with that call and the stack is cleared
     * 
     */
    CommStackMap_p_t  m_commStack;
    /**
     * @brief map of process,rank,thread to the current function call stack. As functions exit they are popped from the stack
     * 
     */
    CallStackMap_p_t  m_callStack;
    /**
     * @brief  map of process,rank,thread to a list of ExecData_t objects which contain entry/exit timestamps for function calls
     * 
     * In practise the call list is purged of completed events each IO step through calls to trimCallList
     */
    CallListMap_p_t   m_callList;
    /**
     * @brief map of function index to an array of complete calls to this function during this IO step
     * 
     * In practise this map is cleared every IO step by calls to trimCallList
     */
    ExecDataMap_t     m_execDataMap;


    /**
     * @brief map of call event index string to the event
     *
     * Completed calls are removed from this list every IO step by calls to trimCallList
     */
    std::unordered_map<std::string, CallListIterator_t> m_callIDMap;
    
    /**
     * @brief verbose
     * 
     */
    bool m_verbose;
  };

} // end of AD namespace
