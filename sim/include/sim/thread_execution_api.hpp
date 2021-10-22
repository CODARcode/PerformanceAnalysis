#include <chimbuko_config.h>
#include "ad.hpp"

namespace chimbuko_sim{
  using namespace chimbuko;


  /**
   * @brief This class implements an alternative API for providing events to the ADsim
   *
   * The class is associated with a single thread. The user adds sequential events corresponding to function entry and exit, and any counters/comms between those calls are associated with the execution
   */
  class threadExecution{
    unsigned long m_tid;
    ADsim &m_ad;

    long m_tlast; //last time  

    struct comm{
      CommType type;
      unsigned long partner_rank;
      unsigned long bytes;
      long t_delta;

      comm(CommType type, unsigned long partner_rank, unsigned long bytes, long t_delta): type(type), partner_rank(partner_rank), bytes(bytes), t_delta(t_delta){}
    };
    struct counter{
      std::string name;
      unsigned long value;
      long t_delta;
    
      counter(const std::string &name, const unsigned long value, const long t_delta): name(name), value(value), t_delta(t_delta){}
    };
    
    struct openFunction{
      std::string func;
      long entry;
      std::string tag;
      bool is_anomaly;
      double score;  

      std::vector<comm> comms;
      std::vector<counter> counters;
      std::vector<CallListIterator_t> children;

      openFunction(const std::string &func, long entry, const std::string &tag, bool is_anomaly, double score): 
	func(func), entry(entry), tag(tag), is_anomaly(is_anomaly), score(score){}
    };
  
    std::stack<openFunction> m_stack;
    std::unordered_map<std::string, CallListIterator_t> m_tagged_events;
  public:

    /**
     * @brief Create a threadExecution instance for a given thread index and ADsim 'rank' instance
     */
    threadExecution(unsigned long tid, ADsim &ad): m_tid(tid), m_ad(ad), m_tlast(-1){}
  
    /**
     * @brief Register the entry event of a non-anomalous function
     * @param func The function name
     * @param ts The timestamp
     * @param tag An optional tag that allows for retrieval of the CallListIterator_t corresponding to the function execution after exit
     */
    void enterNormal(const std::string &func, const long ts, const std::string &tag = "");

    /**
     * @brief Register the entry event of an anomalous function
     * @param func The function name
     * @param ts The timestamp
     * @param score The outlier score
     * @param tag An optional tag that allows for retrieval of the CallListIterator_t corresponding to the function execution after exit
     */
    void enterAnomaly(const std::string &func, const long ts, double score, const std::string &tag = "");

    /**
     * @brief Add a counter to the currently open function execution
     */
    void addCounter(const std::string &name, const unsigned long value, const long ts);

    /**
     * @brief Add a comm event to the currently open function execution
     */ 
    void addComm(CommType type, unsigned long partner_rank, unsigned long bytes, long ts);

    /**
     * @brief Register the exit event of the currently open function. Internally this will poke the function execution onto the ADsim simulator
     */     
    void exit(const long ts);

    /**
     * @brief Retrieve a tagged function execution (must be caled after exit of the function execution)
     */         
    CallListIterator_t getTagged(const std::string &tag) const;      
  };

}
