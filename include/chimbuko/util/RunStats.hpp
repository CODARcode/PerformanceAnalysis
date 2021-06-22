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
    struct State {
      double count; /**< count of instances */
      double eta; /**< mean */
      double rho;
      double tau;
      double phi;
      double min; /**< minimum */
      double max; /**< maximum */
      double acc; /**< sum */
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
      /**
       * @brief Reset state
       */
      void clear()
      {
	count = eta = rho = tau = phi = acc = 0.0;
	min = std::numeric_limits<double>::max();
	max = std::numeric_limits<double>::min();
      }

      /**
       * @brief Serialize using cereal
       */
      template<class Archive>
      void serialize(Archive & archive){
	archive(count, eta, rho, tau, phi, min, max, acc);
      }

      /**
       * @brief Equivalence operator
       */
      bool operator==(const State &r) const{
	return count == r.count && eta == r.eta && rho == r.rho && tau == r.tau  && phi == r.phi && min == r.min && max == r.max && acc == r.acc;
      }

      /**
       * @brief Get this object as a JSON instance
       */
      nlohmann::json get_json() const;
      
      /**
       * @brief Set this object to the values stored in the JSON instance
       */
      void set_json(const nlohmann::json &to);


      /**
       * Serialize into Cereal portable binary format
       */
      std::string serialize_cerealpb() const;
      
      /**
       * Serialize from Cereal portable binary format
       */     
      void deserialize_cerealpb(const std::string &strstate);

    };

    /**
     * @brief A serializable object containing the stats values
     *
     */
    struct RunStatsValues {
      double count;
      double minimum;
      double maximum;
      double accumulate;
      double mean;
      double stddev;
      double skewness;
      double kurtosis;

      /**
       * @brief Serialize using cereal
       */
      template<class Archive>
      void serialize(Archive & archive){
	archive(count, minimum, maximum, accumulate, mean, stddev, skewness, kurtosis);
      }

      /**
       * @brief Comparison operator
       */
      bool operator==(const RunStatsValues &r) const{ 
	return count == r.count && minimum == r.minimum && maximum == r.maximum && accumulate == r.accumulate &&
	  mean == r.mean && stddev == r.stddev && skewness == r.skewness && kurtosis == r.kurtosis;
      }

    };

    /**
     * @brief Constructor
     * @param do_accumulate If true the sum of the provided values will also be collected
     */
    RunStats(bool do_accumulate = false);
    ~RunStats();

    /**
     * @brief Reset the statistics
     */
    void clear();

    /**
     * @brief Return the current set of internal variables (state) as an instance of State
     */
    const State &get_state() const{ return m_state; }

    /**
     * @brief Set the internal variables from an instance of State
     */
    void set_state(const State& s);

    /**
     * @brief Create an instance of this class from a State instance
     */
    static RunStats from_state(const State& s) {
      RunStats stats;
      stats.set_state(s);
      return stats;
    }

    /**
     * @brief Create a RunStats instance from the current state
     */
    RunStats copy() {
      return from_state(get_state());
    }

    /**
     * @brief Return the current set of internal variables (state) as a JSON object
     */
    nlohmann::json get_json_state() const;

    /**
     * @brief Set the internal variables from a JSON object
     */
    void set_json_state(const nlohmann::json& s);

    /**
     * @brief Create a RunStats instance from a JSON object
     */
    static RunStats from_json_state(const nlohmann::json& s) {
      RunStats stats;
      stats.set_json_state(s);
      return stats;
    }

    /**
     * @brief Get the current set of internal variables (state) as a JSON-formatted string
     */
    std::string get_strstate();

    /**
     * @brief Set the state from a JSON-formatted string
     */
    void set_strstate(const std::string& s);

    /**
     * @brief Create an instance of RunStats from a JSON-formatted string
     */
    static RunStats from_strstate(const std::string& s) {
      RunStats stats;
      stats.set_strstate(s);
      return stats;
    }

    /**
     * @brief Serialize this class for communication over the network
     */
    std::string net_serialize() const;

    /**
     * @brief Unserialize this class after communication over the network
     */
    void net_deserialize(const std::string &s);

    /**
     * @brief Add a new value to be included in internal statistics
     */
    void push(double x);

    /**
     * @brief Get the number of values added to the statistics
     */
    double count() const;
    double minimum() const;
    double maximum() const;

    /**
     * @brief If m_do_accumulate, the accumulated sum of all values added, otherwise 0
     */
    double accumulate() const;
    double mean() const;
    double variance(double ddof=1.0) const;
    double stddev(double ddof=1.0) const;
    double skewness() const;
    double kurtosis() const;

    /**
     * @brief Set whether the sum of all values is to be maintained
     */
    void set_do_accumulate(bool do_accumulate) { m_do_accumulate = do_accumulate; }

    /**
     * @brief Get the current statistics as a JSON object
     */
    nlohmann::json get_json() const;

    /**
     * @brief Get the current statistics as a RunStatsValues object
     */
    RunStatsValues get_stat_values() const;


    /**
     * @brief Combine two RunStats instances such that the resulting statistics are the union of the two
     */
    friend RunStats operator+(const RunStats a, const RunStats b);

    /**
     * @brief Combine two RunStats instances such that the resulting statistics are the union of the two
     */
    RunStats& operator+=(const RunStats& rs);

    /**
     * @brief Comparison operator
     */
    friend bool operator==(const RunStats& a, const RunStats& b);

    /**
     * @brief Negative comparison operator
     */
    friend bool operator!=(const RunStats& a, const RunStats& b);

  private:
    State m_state; /**< The internal variables */
    bool m_do_accumulate; /**< True if the sum of the input values are maintained */
  };

  RunStats operator+(const RunStats a, const RunStats b);
  bool operator==(const RunStats& a, const RunStats& b);
  bool operator!=(const RunStats& a, const RunStats& b);

  /**
   * @brief The mean of the data vector of N data normalized by N-ddof
   */
  double static_mean(const std::vector<double>& data, double ddof=1.0);

  /**
   * @brief The mean of the data vector of N data, with variance normalized by N-ddof
   */
  double static_std(const std::vector<double>& data, double ddof=1.0);

} // end of chimbuko namespace
