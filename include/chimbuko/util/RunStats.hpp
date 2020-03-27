#pragma once
#include <iostream>
#include <vector>
#include <limits>
#include <nlohmann/json.hpp>

namespace chimbuko {

  /**
   * @brief Compute statistics in a single pass
   * 
   * Computes the minimum, maximum, mean, variance, standard deviation,
   * skewness, and kurtosis. Optionally, also computes accumulated values.
   * 
   * RunStats objects may also be added together and copied.
   * 
   * Based entirely on the C++ code by John D Cook at
   * http://www.johndcook.com/skewness_kurtosis.html
   * 
   */
  class RunStats {
  public:
    /**
     * @brief Internal state of RunStats object
     * 
     */
    typedef struct State {
      double count;
      double eta;
      double rho;
      double tau;
      double phi;
      double min;
      double max;
      double acc;
      State()
      {
	clear();
      }
      State(
            double _count, 
            double _eta, double _rho, double _tau, double _phi, 
            double _min, double _max, double _acc)
        : count(_count), 
          eta(_eta), rho(_rho), tau(_tau), phi(_phi),
          min(_min), max(_max), acc(_acc)
      {

      }
      void clear()
      {
	count = eta = rho = tau = phi = acc = 0.0;
	min = std::numeric_limits<double>::max();
	max = std::numeric_limits<double>::min();
      }
    } State;

  public:
    RunStats(bool do_accumulate = false);
    ~RunStats();

    void clear();
    
    State get_state() { return m_state; }
    void set_state(const State& s);
    static RunStats from_state(const State& s) {
      RunStats stats;
      stats.set_state(s);
      return stats;
    }
    RunStats copy() {
      return from_state(get_state());
    }

    std::string get_strstate();
    void set_strstate(const std::string& s);
    static RunStats from_strstate(const std::string& s) {
      RunStats stats;
      stats.set_strstate(s);
      return stats;
    }

    /**
     * @brief Add a new value to be included in internal statistics
     */
    void push(double x);

    double count() const;
    double minimum() const;
    double maximum() const;
    double accumulate() const;
    double mean() const;
    double variance(double ddof=1.0) const;
    double stddev(double ddof=1.0) const;
    double skewness() const;
    double kurtosis() const;
    
    void set_do_accumulate(bool do_accumulate) { m_do_accumulate = do_accumulate; }

    nlohmann::json get_json() const;
    nlohmann::json get_json_state() const;
    
    friend RunStats operator+(const RunStats a, const RunStats b);
    RunStats& operator+=(const RunStats& rs);

    friend bool operator==(const RunStats& a, const RunStats& b);
    friend bool operator!=(const RunStats& a, const RunStats& b);

  private:
    State m_state;
    bool m_do_accumulate;
  };

  RunStats operator+(const RunStats a, const RunStats b);
  bool operator==(const RunStats& a, const RunStats& b);
  bool operator!=(const RunStats& a, const RunStats& b);

  double static_mean(const std::vector<double>& data, double ddof=1.0);
  double static_std(const std::vector<double>& data, double ddof=1.0);

} // end of chimbuko namespace
