#pragma once
#include <chimbuko_config.h>
#include <chimbuko/ad/ADNetClient.hpp>
#include <chimbuko/ad/ExecData.hpp>
#include <chimbuko/util/RunStats.hpp>

namespace chimbuko{
 
  /**
   * @brief Aggregated anomaly metrics for a single function
   */
  class FuncAnomalyMetrics{
  public:

    /**
     * @brief Class state that is serialized for pserver comms
     */
    struct State{
      RunStats::State score;
      RunStats::State severity;
      int count;
      int fid;
      std::string func;

      State(const FuncAnomalyMetrics &parent): score(parent.m_score.get_state()), severity(parent.m_severity.get_state()), count(parent.m_count), fid(parent.m_fid), func(parent.m_func){}
      State(): count(0){}
      
      /*
       * @brief Serialize this instance in Cereal
       */
      template<class Archive>
      void serialize(Archive & archive){
	archive(score,severity,count,fid,func);
      }
    };

    FuncAnomalyMetrics(): m_count(0), m_score(true), m_severity(true), m_fid(-1){}
    FuncAnomalyMetrics(const int fid, const std::string &func): m_fid(fid), m_func(func), m_count(0), m_score(true), m_severity(true){}

    /**
     * @brief Get the current state as a state object
     *
     * The string dump of this object is the serialized form sent to the parameter server
     */    
    State get_state() const;


    /**
     * @brief Set the internal variables to the given state object
     */
    void set_state(const State &s);


    /**
     * @brief Add data associated with an anomalous event
     */
    void add(const ExecData_t &event);


    /**
     * @brief Equivalence operator
     */
    bool operator==(const FuncAnomalyMetrics &r) const{
      return m_score == r.m_score && m_severity == r.m_severity && m_count == r.m_count && m_fid == r.m_fid && m_func == r.m_func;
    }

    /**
     * @brief Inequality operator
     */
    inline bool operator!=(const FuncAnomalyMetrics &r) const{ return !(*this == r); }

    const RunStats &get_score() const{ return m_score; }
    const RunStats &get_severity() const{ return m_severity; }
    int get_count() const{ return m_count; }
    int get_fid() const{ return m_fid; }
    const std::string & get_func() const{ return m_func; }
    
  private:
    RunStats m_score; /**< Anomaly scores */
    RunStats m_severity; /**< Anomaly severity */
    int m_count; /**< The number of anomalies */
    int m_fid; /**< Function index */
    std::string m_func; /**< Function name */

  };

};
