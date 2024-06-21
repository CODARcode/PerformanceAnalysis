#pragma once
#include <chimbuko_config.h>
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
     * @brief Serialize using cereal, for example as part of a compound object
     */
    template<class Archive>
    void serialize(Archive & archive){
      archive(m_count, m_eta, m_rho, m_tau, m_phi, m_min, m_max, m_acc, m_do_accumulate);
    }

    /**
     * Serialize into Cereal portable binary format
     */
    std::string serialize_cerealpb() const;
    
    /**
     * Serialize from Cereal portable binary format
     */     
    void deserialize_cerealpb(const std::string &strstate);
    
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

    /**
     * @brief Return the variance of the data
     *
     * If ddof=1 (default) the variance will include Bessel's correction, and represents an estimate of the population variance.
     * If ddof=0 the variance will be the variance of the sample
     */
    double variance(double ddof=1.0) const;
    double stddev(double ddof=1.0) const;
    double skewness() const;
    double kurtosis() const;

    /**
     * @brief Set whether the sum of all values is to be maintained
     */
    void set_do_accumulate(bool do_accumulate) { m_do_accumulate = do_accumulate; }

    /**
     * @brief Determine whether the sum of all values is to be maintained
     */
    bool get_do_accumulate() const{ return m_do_accumulate; }

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
    friend RunStats operator+(const RunStats &a, const RunStats &b);

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
    
    /**
     * @brief Test for equivalence up to a fixed tolerance allowing for finite-precision errors
     */
    bool equiv(const RunStats &b) const;

    /**
     * @brief Set the eta (mean) parameter
     */
    void set_eta(double to){ m_eta = to; }
    /**
     * @brief Set the rho parameter (variance * [count-1])
     */
    void set_rho(double to){ m_rho = to; }
    /**
     * @brief Set the count parameter
     */
    void set_count(double to){ m_count = to; }

  protected:
    /* Note the variables in https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance are M2,M3,M4. The mappings are provided in the comments below.*/
    double m_count; /**< count of instances */
    double m_eta; /**< mean */
    double m_rho; /**< = M2 = \sum_i (x_i - \bar x)^2 */
    double m_tau; /**< = M3 = \sum_i (x_i - \bar x)^3 */
    double m_phi; /**< = M4 = \sum_i (x_i - \bar x)^4 */
    double m_min; /**< minimum */
    double m_max; /**< maximum */
    double m_acc; /**< sum */
    bool m_do_accumulate; /**< True if the sum of the input values are maintained */
  };

  RunStats operator+(const RunStats &a, const RunStats &b);
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
