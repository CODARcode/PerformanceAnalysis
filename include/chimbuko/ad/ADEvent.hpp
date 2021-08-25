#pragma once
#include <chimbuko_config.h>
#include "chimbuko/ad/ADDefine.hpp"
#include "chimbuko/ad/ExecData.hpp"
#include "chimbuko/ad/ADMetadataParser.hpp"
#include "chimbuko/util/map.hpp"
#include <string>
#include <vector>
#include <list>
#include <stack>
#include <unordered_map>
#include <tuple>
#include <sstream>

namespace chimbuko {
  /**
   * @brief a stack of CommData_t
   *
   */
  typedef std::stack<CommData_t> CommStack_t;

  /**
   * @brief map of process, rank, thread -> Commstack_t
   */
  typedef mapPRT<CommStack_t> CommStackMap_p_t;

  /**
   * @brief a stack of CounterData_t
   *
   */
  typedef std::stack<CounterData_t> CounterStack_t;

  /**
   * @brief map of process, rank, thread -> Counterstack_t
   */
  typedef mapPRT<CounterStack_t> CounterStackMap_p_t;

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
  typedef mapPRT<CallList_t> CallListMap_p_t;


  /**
   * @brief function call stack
   */
  typedef std::stack<CallListIterator_t> CallStack_t;

  /**
   * @brief map of process, rank, thread -> CallStack_t
   */
  typedef mapPRT<CallStack_t> CallStackMap_p_t;


  /**
   * @brief hash map of a collection of ExecData_t per function
   *
   * key is function id and value is a vector of CallListIterator_t (i.e. ExecData_t)
   *
   */
  typedef std::unordered_map<unsigned long, std::vector<CallListIterator_t>> ExecDataMap_t;

  /**
   * @brief A type that stores some information about an event whose data may have been deleted
   */
  struct EventInfo{
    eventID id;
    EventDataType type;
    unsigned long ts;

    EventInfo(){}

    /**
     * @brief Create from an Event_t
     */
    EventInfo(const Event_t &e){
      id = e.id();
      ts = e.ts();
      type = e.type();
    }

    /**
     * @brief Create from an Event_t
     * @param entry_or_exit 0:entry 1:exit
     */
    EventInfo(const ExecData_t &e, int entry_or_exit){
      id = e.get_id();
      ts = entry_or_exit == 0 ? e.get_entry() : e.get_exit();
      type = EventDataType::FUNC;
    }

    std::string print() const{
      std::stringstream os;
      os << "{";
      switch(type){
      case EventDataType::FUNC:
	os << "FUNC, "; break;
      case EventDataType::COMM:
	os << "COMM, "; break;
      case EventDataType::COUNT:
	os << "COUNT, "; break;
      case EventDataType::Unknown:
	throw std::runtime_error("Unknown EventDataType");
      }
      os << id.toString() << ", " << ts << "}";
      return os.str();
    }



  };

  
  /**
   * @brief An abstract interface for obtaining events given an event index
   */
  class ADEventIDmap{
  public:
    /**
     * @brief Get an iterator to an ExecData_t instance with given event index string
     *
     * throws a runtime error if the call is not present in the call-list
     */
    virtual CallListIterator_t getCallData(const eventID &event_id) const = 0;

    /**
     * @brief Get a pair of iterators marking the start and one-past-the-end of a window of size (up to) win_size events
     *        on either size around the given event occurring on the same thread
     */
    virtual std::pair<CallListIterator_t, CallListIterator_t> getCallWindowStartEnd(const eventID &event_id, const int win_size) const = 0;
    
    virtual ~ADEventIDmap(){}
  };


  /**
   * @brief Event manager whose role is to correlate function entry and exit events and associate other counters with the function call
   *
   * When a function call with ENTRY signature is inserted, the event is placed on the call stack for that thread. Events associated with MPI comms and counters are also placed
   * on their respective stacks. When a function call with EXIT signature on the same thread is inserted, a complete call is generated and placed in the call list, and all comm and
   * counter events on their stacks are associated with that call.
   */
  class ADEvent: public ADEventIDmap {

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
     * @brief copy a pointer that is externally defined function map object
     *
     * @param m counter map object
     */
    void linkCounterMap(const std::unordered_map<int, std::string>* m) { m_counterMap = m; }


    /**
     * @brief Optional: give the event manager knowledge of which threads are GPU threads, improves error checking
     */
    void linkGPUthreadMap(const std::unordered_map<unsigned long, GPUvirtualThreadInfo> *m){ m_gpu_thread_Map = m; }


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
     * @brief Get the Counter name object
     *
     * @return const std::unordered_map<int, std::string>* pointer to counter name object
     */
    const std::unordered_map<int, std::string>* getCounterMap() const { return m_counterMap; }


    /**
     * @brief Get the Exec Data Map object ( map of function id -> vector of iterators to ExecData objects )
     *
     * @return const ExecDataMap_t* pointer to ExecDataMap_t object
     */
    const ExecDataMap_t* getExecDataMap() const { return &m_execDataMap; }

    /**
     * @brief Get the Call List Map object ( map of pid/rid/tid -> list of ExecData objects )
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
     *
     * throws a runtime error if the call is not present in the call-list
     */
    CallListIterator_t getCallData(const eventID &event_id) const override;

    /**
     * @brief Get a pair of iterators marking the start and one-past-the-end of a window of size (up to) win_size events
     *        on either size around the given event occurring on the same thread
     */
    std::pair<CallListIterator_t, CallListIterator_t> getCallWindowStartEnd(const eventID &event_id, const int win_size) const override;

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
     * @brief add a counter event
     *
     * @param event counter event
     * @return EventError event error code
     */
    EventError addCounter(const Event_t& event);


    /**
     * @brief Add a complete function call, primarily for testing
     *
     * @param exec Instance of ExecData_t
     * @return Iterator to inserted call
     */
    CallListIterator_t addCall(const ExecData_t &exec);


    /**
     * @brief trim out all function calls that are completed (i.e. a pair of ENTRY and EXIT events are observed)
     * @param n_keep_thread The amount of events per thread to maintain [if they exist] (allows window view to extend into previous io step)
     * @return CallListMap_p_t* trimed function calls
     */
    CallListMap_p_t* trimCallList(int n_keep_thread = 0);

    /**
     * @brief Get the total number of function events in the call list over all pid/rid/tid
     */
    size_t getCallListSize() const;

    /**
     * @brief purge all function calls that are completed (i.e. a pair of ENTRY and EXIT events are observed)
     * @param n_keep_thread The amount of events per thread to maintain [if they exist] (allows window view to extend into previous io step)
     *
     * Functionality is the same as trimCallList only it doesn't return the trimmed function calls
     */
    void purgeCallList(int n_keep_thread = 0);

    /**
     * @brief show current call stack tree status
     *
     * @param verbose true to see all details
     */
    void show_status(bool verbose=false) const;

    /**
     * @brief Get the map of correlation ID to event for those events that have yet to be partnered
     */
    const std::unordered_map<unsigned long, CallListIterator_t> & getUnmatchCorrelationIDevents() const{ return  m_unmatchedCorrelationID; }

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
     * @brief pointer to map of counter index to counter name string
     *
     */
    const std::unordered_map<int, std::string> *m_counterMap;

    int m_eidx_func_entry; /**< If previously seen, the eid corresponding to the function entry event (-1 otherwise)*/
    int m_eidx_func_exit; /**< If previously seen, the eid corresponding to the function exit event (-1 otherwise)*/
    int m_eidx_comm_send; /**< If previously seen, the eid corresponding to the comm send event (-1 otherwise)*/
    int m_eidx_comm_recv; /**< If previously seen, the eid corresponding to the comm recv event (-1 otherwise)*/


    /**
     * @brief Optional: give the event manager knowledge of which threads are GPU threads, improves error checking
     */
    const std::unordered_map<unsigned long, GPUvirtualThreadInfo> *m_gpu_thread_Map;


    /**
     * @brief communication event stack. Once a function call has exited, all comms events are associated with that call and the stack is cleared
     *
     */
    CommStackMap_p_t  m_commStack;

    /**
     * @brief map of process,rank,thread to counter events. Once a function call has exited, all counter events are associated with that call and the stack is cleaned.
     */
    CounterStackMap_p_t m_counterStack;

    /**
     * @brief map of process,rank,thread to the current function call stack. As functions exit they are popped from the stack
     *
     */
    CallStackMap_p_t  m_callStack;
    /**
     * @brief  map of process,rank,thread to a list of ExecData_t objects which contain entry/exit timestamps for function calls
     *
     * In practise the call list is purged of completed events each IO step through calls to trimCallList unless those elements are marked as non-deletable
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
    std::unordered_map<eventID, CallListIterator_t> m_callIDMap;


    /**
     * @brief Events with unmatched correlation IDs
     *
     * Events that correspond to GPU kernel launches and executions are given correlation IDs as
     * counters that allow us to match the CPU thread that launched them to the GPU kernel event
     */
    std::unordered_map<unsigned long, CallListIterator_t> m_unmatchedCorrelationID;

    /**
     * @brief Map of event index to the number of unmatched correlation IDs
     */
    std::unordered_map<eventID,size_t> m_unmatchedCorrelationID_count;

    /**
     * @brief verbose
     *
     */
    bool m_verbose;

    /**
     * @brief  Check if the event has a correlation ID counter, if so try to match it to an outstanding unmatched
     *         event with a correlation ID
     */
    void checkAndMatchCorrelationID(CallListIterator_t it);

    /**
     * @brief Flag the call and all it's parental line such that they are protected from deletion by the garbage collection
     */
    void stackProtectGC(CallListIterator_t it);

    /**
     * @brief Flag the call and all it's parental line such that they are not protected from deletion by the garbage collection,
     *        stopping if a call with an unmatched correlation ID is encountered
     */
    void stackUnProtectGC(CallListIterator_t it);
  };

} // end of AD namespace
