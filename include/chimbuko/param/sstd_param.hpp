#pragma once

#include "chimbuko/param.hpp"
#include "chimbuko/util/RunStats.hpp"
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <vector>
#include <iostream>


namespace chimbuko {

  /**
   * @@brief Implementation of ParamInterface for anomaly detection based on function time distribution (mean, std. dev., etc)
   */
  class SstdParam : public ParamInterface
  {
  public:
    SstdParam();
    ~SstdParam();

    /**
     * @brief Clear all statistics
     */
    void clear() override;

    /**
     * @brief Get the number of functions for which statistics are being collected
    */
    size_t size() const override { return m_runstats.size(); }

    /**
     * @brief Convert internal run statistics to string format for IO
     * @return Run statistics in string format
     */
    std::string serialize() const override;

    /**
     * @brief Update the internal run statistics with those included in the serialized input map
     * @param parameters The parameters in serialized format
     * @param return_update Indicates that the function should return a serialized copy of the updated parameters
     * @return An empty string or a serialized copy of the updated parameters depending on return_update
     */
    std::string update(const std::string& parameters, bool return_update=false) override;

    /**
     * @brief Set the internal run statistics to match those included in the serialized input map. Overwrite performed only for those keys in input.
     * @param runstats The serialized input map
     */
    void assign(const std::string& parameters) override;

    void show(std::ostream& os) const override;

    /**
     * @brief Convert a run statistics mapping into a JSON string
     * @param The map between global function index and statistics
     * @return Run statistics in string format
     */
    static std::string serialize_json(const std::unordered_map<unsigned long, RunStats>& runstats);

    /**
     * @brief Convert a run statistics JSON string into a map
     * @param[in] parameters The parameter string
     * @param[out] runstats The map between global function index and statistics
     */
    static void deserialize_json(const std::string& parameters,
			    std::unordered_map<unsigned long, RunStats>& runstats);


    /**
     * @brief Convert a run statistics mapping into a Cereal portable binary representration
     * @param The run stats mapping
     * @return Run statistics in string format
     */
    static std::string serialize_cerealpb(const std::unordered_map<unsigned long, RunStats>& runstats);

    /**
     * @brief Convert a run statistics Cereal portable binary representation string into a map
     * @param[in] parameters The parameter string
     * @param[out] runstats The map between global function index and statistics
     */
    static void deserialize_cerealpb(const std::string& parameters,
			    std::unordered_map<unsigned long, RunStats>& runstats);


    /**
     * @brief Update the internal run statistics with those included in the input map
     * @param[in] runstats The map between global function index and statistics
     */
    void update(const std::unordered_map<unsigned long, RunStats>& runstats);

    /**
     * @brief Update the internal statistics with those included in another SstdParam instance.
     * @param[in] other The other SstdParam instance
     */
    void update(const SstdParam& other) { update(other.m_runstats); }

    /**
     * @brief Update the internal run statistics with those included in the input map. Input map is then updated to reflect new state.
     * @param[in,out] runstats The map between global function index and statistics
     */
    void update_and_return(std::unordered_map<unsigned long, RunStats>& runstats);

    /**
     * @brief Update the internal statistics with those included in another SstdParam instance. Other SstdParam is then updated to reflect new state.
     * @param[in,out] other The other SstdParam instance
     */
    void update_and_return(SstdParam& other) { update_and_return(other.m_runstats); }

    /**
     * @brief Set the internal run statistics to match those included in the input map. Overwrite performed only for those keys in input.
     * @param runstats The input map between global function index and statistics
     */
    void assign(const std::unordered_map<unsigned long, RunStats>& runstats);

    /**
     * @brief Get an element of the internal map
     * @param id The global function index
     */
    RunStats& operator [](unsigned long id) { return m_runstats[id]; }

    /**
     * @brief Get the internal map between global function index and statistics
     */
    const std::unordered_map<unsigned long, RunStats> & get_runstats() const{ return m_runstats; }
    /**
     * @brief Get the algorithm parameters associated with a given function
     */
    nlohmann::json get_algorithm_params(const unsigned long func_id) const override;


  protected:
    /**
     * @brief Get the internal map of global function index to statistics
     */
    std::unordered_map<unsigned long, RunStats> & access_runstats(){ return m_runstats; }

  private:
    std::unordered_map<unsigned long, RunStats> m_runstats; /**< Map of global function index to statistics*/
  };



  /**
   * @brief Histogram Implementation
   */
  class Histogram {

  public:
    Histogram();
    ~Histogram();

    struct Data {
      //std::vector<double> runtimes;
      double glob_threshold;
      std::vector<int> counts;
      std::vector<double> bin_edges;

      Data(){
        clear();
      }
      Data(double& g_threshold, std::vector<int>& h_counts, std::vector<double>& h_bin_edges ) {
        glob_threshold = g_threshold;
        counts = h_counts;
        bin_edges = h_bin_edges;
      }

      void clear() {
        glob_threshold = -1 * log2(1.00001);
        counts.clear();
        bin_edges.clear();
      }

      /**
       * @brief Serialize using cereal
       */
      template<class Archive>
      void serialize(Archive & archive){
         archive(glob_threshold, counts, bin_edges);
      }



    };

    //friend class cereal::access;


    void clear();

    void push (double x);

    const Data &get_histogram() const{ return m_histogram; } //const Data &get_histogram() const{ return m_histogram; }

    /**
     * @brief Set the internal variables from an instance of Histogram Data
     */
    void set_hist_data(const Data& d);

    /**
     * @brief Create an instance of this class from a Histogram Data instance
     */
    static Histogram from_hist_data(const Data& d) {
      Histogram histdata;
      histdata.set_hist_data(d);
      return histdata;
    }

    void create_histogram(std::vector<double>& runtimes);

    void merge_histograms(Histogram& g, std::vector<double>& runtimes);
    /**
     * @brief Combine two Histogram instances such that the resulting statistics are the union of the two
     */
    Histogram combine_two_histograms(const Histogram& a, const Histogram& b); //friend Histogram operator+(const Histogram a, const Histogram b);

    /**
     * @brief Combine two Histogram instances such that the resulting statistics are the union of the two
     */
    Histogram& operator+=(const Histogram& rs);

    double _scott_binWidth(std::vector<double> & vals);


    void set_glob_threshold(double& l) { m_histogram.glob_threshold = l;}
    void set_counts(std::vector<int>& c) { m_histogram.counts = c; }
    void set_bin_edges(std::vector<double>& be) {m_histogram.bin_edges = be;}
    void add2counts(int& count) {m_histogram.counts.push_back(count);}
    void add2binedges(double& bin_edge) {m_histogram.bin_edges.push_back(bin_edge);}

    double& get_threshold() {return m_histogram.glob_threshold;}
    /**
     * @brief Get the current statistics as a JSON object
     */
    nlohmann::json get_json() const;

  private:
    Data m_histogram;
  };

  /**
   * @@brief Implementation of ParamInterface for HBOS based anomaly detection
   */
  class HbosParam : public ParamInterface {
  public:
    HbosParam();
    ~HbosParam();
    /**
     * @brief Clear all statistics
     */
    void clear() override;

    const int find(const unsigned long& func_id);

    /**
     * @brief Get the number of functions for which statistics are being collected
    */
    size_t size() const override { return m_hbosstats.size(); }

    /**
     * @brief Convert internal run statistics to string format for IO
     * @return Run statistics in string format
     */
    std::string serialize() const override;

    /**
     * @brief Update the internal run statistics with those included in the serialized input map
     * @param parameters The parameters in serialized format
     * @param return_update Controls return format
     * @return An empty string if return_update==False, otherwise the serialized updated parameters
     */
    std::string update(const std::string& parameters, bool return_update=false) override;

    /**
     * @brief Set the internal run statistics to match those included in the serialized input map. Overwrite performed only for those keys in input.
     * @param runstats The serialized input map
     */
    void assign(const std::string& parameters) override;

    void show(std::ostream& os) const override;

    /**
     * @brief Set the internal run statistics to match those included in the input map. Overwrite performed only for those keys in input.
     * @param runstats The input map between global function index and statistics
     */
    void assign(const std::unordered_map<unsigned long, Histogram>& hbosstats);

    /**
     * @brief Convert a run statistics mapping into a Cereal portable binary representration
     * @param The run stats mapping
     * @return Run statistics in string format
     */
    static std::string serialize_cerealpb(const std::unordered_map<unsigned long, Histogram>& hbosstats);

    /**
     * @brief Get an element of the internal map
     * @param id The global function index
     */
    Histogram& operator [](unsigned long id) { return m_hbosstats[id]; }

    /**
     * @brief Convert a run statistics Cereal portable binary representation string into a map
     * @param[in] parameters The parameter string
     * @param[out] runstats The map between global function index and statistics
     */
    static void deserialize_cerealpb(const std::string& parameters,
			    std::unordered_map<unsigned long, Histogram>& hbosstats);


    /**
     * @brief Update the internal run statistics with those included in the input map
     * @param[in] runstats The map between global function index and statistics
     */
    void update(const std::unordered_map<unsigned long, Histogram>& hbosstats);

    /**
     * @brief Update the internal statistics with those included in another SstdParam instance.
     * @param[in] other The other SstdParam instance
     */
    void update(const HbosParam& other) { update(other.m_hbosstats); }

    /**
     * @brief Update the internal run statistics with those included in the input map. Input map is then updated to reflect new state.
     * @param[in,out] runstats The map between global function index and statistics
     */
    void update_and_return(std::unordered_map<unsigned long, Histogram>& hbosstats);

    /**
     * @brief Update the internal statistics with those included in another SstdParam instance. Other SstdParam is then updated to reflect new state.
     * @param[in,out] other The other SstdParam instance
     */
    void update_and_return(HbosParam& other) { update_and_return(other.m_hbosstats); }


    nlohmann::json get_algorithm_params(const unsigned long func_id) const override;
    //nlohmann::json get_function_stats(const unsigned long func_id) const override;
  private:
    std::unordered_map<unsigned long, Histogram> m_hbosstats;
  };


} // end of chimbuko namespace
