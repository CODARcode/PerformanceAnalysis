#pragma once
#include <chimbuko_config.h>
#include <chimbuko/ad/ADNetClient.hpp>
#include <chimbuko/ad/ExecData.hpp>
#include <chimbuko/core/util/RunStats.hpp>

namespace chimbuko{
 
  /**
   * @brief Aggregated anomaly metrics for a single function
   */
  class FuncAnomalyMetrics{
  public:
    FuncAnomalyMetrics(): m_count(0), m_score(true), m_severity(true), m_fid(-1){}
    FuncAnomalyMetrics(const int fid, const std::string &func): m_fid(fid), m_func(func), m_count(0), m_score(true), m_severity(true){}

    /*
     * @brief Serialize this instance in Cereal
     */
    template<class Archive>
    void serialize(Archive & archive){
      archive(m_score,m_severity,m_count,m_fid,m_func);
    }

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
    void set_score(const RunStats &to){ m_score = to; }

    const RunStats &get_severity() const{ return m_severity; }
    void set_severity(const RunStats &to){ m_severity = to; }

    int get_count() const{ return m_count; }
    void set_count(const int to){ m_count = to; }

    int get_fid() const{ return m_fid; }
    void set_fid(const int to){ m_fid = to; }

    const std::string & get_func() const{ return m_func; }
    void set_func(const std::string &to){ m_func = to; }

  private:
    RunStats m_score; /**< Anomaly scores */
    RunStats m_severity; /**< Anomaly severity */
    int m_count; /**< The number of anomalies */
    int m_fid; /**< Function index */
    std::string m_func; /**< Function name */

  };

};
