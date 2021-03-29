#pragma once

#include "chimbuko/param.hpp"
#include "chimbuko/util/RunStats.hpp"
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <vector>
#include <iostream>

namespace chimbuko {

  /**
   * @brief Histogram Implementation
   */
  class Histogram {

  public:
    Histogram();
    ~Histogram();

    struct Data {

      double glob_threshold; /**< global threshold used to filter anomalies*/
      std::vector<int> counts; /**< Bin counts in Histogram*/
      std::vector<double> bin_edges; /**< Bin edges in Histogram*/

      Data(){
        clear();
      }
      Data(const double& g_threshold, const std::vector<int>& h_counts, const std::vector<double>& h_bin_edges ) {
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


    void clear() {m_histogram.clear();}

    void push (double x);

    const Data &get_histogram() const{ return m_histogram; }

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
    /**
     * @brief Create new histogram locally for AD module's batch data instances
     */
    void create_histogram(const std::vector<double>& r_times);


    void merge_histograms(const Histogram& g, const std::vector<double>& runtimes);

    /**
     * @brief Combine two Histogram instances such that the resulting statistics are the union of the two
     */
    //Histogram combine_two_histograms(const Histogram& g, const Histogram& l);
    void combine_two_histograms(Histogram& g, const Histogram& l);

    /**
     * @brief Combine two Histogram instances such that the resulting statistics are the union of the two
     */
    Histogram & operator+=(const Histogram& h);


    /**
     * @brief setd global threshold for anomaly filtering
     */
    void set_glob_threshold(const double& l) { m_histogram.glob_threshold = l;}

    /*
     * @brief set bin counts in Histogram
     */
    void set_counts(std::vector<int> c) { m_histogram.counts = c; }

    /*
     * @brief set bin edges in Histogram
     */
    void set_bin_edges(const std::vector<double>& be) {m_histogram.bin_edges = be;}

    /*
     * @brief Update counts in Histogram
     */
    void add2counts(const int& count) {m_histogram.counts.push_back(count);}

    /*
     * @brief Update counts for a given index of bin in histogram
     */
    void add2counts(const int& id, const int& count) {m_histogram.counts[id] += count;}

    /*
     * @brief Update bin edges in histogram
     */
    void add2binedges(const double& bin_edge) {m_histogram.bin_edges.push_back(bin_edge);}

    /*
     * @brief get global threshold from global histogram
     */
    const double& get_threshold() const {return m_histogram.glob_threshold;}

    /*
     * @brief Get bin counts of Histogram
     */
    const std::vector<int>& counts() const {return m_histogram.counts;}

    /*
     * @brief Get bin edges of histogram
     */
    const std::vector<double>& bin_edges() const {return m_histogram.bin_edges;}

    /**
     * @brief Get the current statistics as a JSON object
     */
    nlohmann::json get_json() const;

  private:
    Data m_histogram; /**< Histogram Data*/

    /*
     * @brief Compute bin width based on Scott's rule
     * @param vals: vector of runtimes
     */
    static double _scott_binWidth(const std::vector<double> & vals);

    /*
     * @brief Compute bin width based on Scott's rule
     * @param global_counts: bin counts in global histogram on pserver
     * @param global_edges: bin edges in global histogram on pserver
     * @param local_counts: bin counts in local histogram in AD module
     * @param local_edges: bin edges in local histogram in AD module
     */
    static double _scott_binWidth(const std::vector<int> & global_counts, const std::vector<double> & global_edges, const std::vector<int> & local_counts, const std::vector<double> & local_edges);

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
     * @brief Get the internal map between global function index and statistics
     */
    const std::unordered_map<unsigned long, Histogram> & get_hbosstats() const{ return m_hbosstats; }

    /**
     * @brief Get the number of functions for which statistics are being collected
    */
    size_t size() const override { return m_hbosstats.size(); }

    /**
     * @brief Convert internal Histogram to string format for IO
     * @return Histogram in string format
     */
    std::string serialize() const override;

    /**
     * @brief Update the internal Histogram with those included in the serialized input map
     * @param parameters The parameters in serialized format
     * @param return_update Controls return format
     * @return An empty string if return_update==False, otherwise the serialized updated parameters
     */
    std::string update(const std::string& parameters, bool return_update=false) override;

    /**
     * @brief Set the internal Histogram to match those included in the serialized input map. Overwrite performed only for those keys in input.
     * @param parameters The serialized input map
     */
    void assign(const std::string& parameters) override;

    void show(std::ostream& os) const override;

    /**
     * @brief Set the internal Histogram to match those included in the input map. Overwrite performed only for those keys in input.
     * @param hbosstats The input map between global function index and Histogram
     */
    void assign(const std::unordered_map<unsigned long, Histogram>& hbosstats);

    /**
     * @brief Convert a Histogram mapping into a Cereal portable binary representration
     * @param hbosstats The Histogram mapping
     * @return Histogram in string format
     */
    static std::string serialize_cerealpb(const std::unordered_map<unsigned long, Histogram>& hbosstats);

    /**
     * @brief Get an element of the internal map
     * @param id The global function index
     */
    Histogram& operator [](unsigned long id) { return m_hbosstats[id]; }

    /**
     * @brief Convert a Histogram Cereal portable binary representation string into a map
     * @param[in] parameters The parameter string
     * @param[out] hbosstats The map between global function index and histogram statistics
     */
    static void deserialize_cerealpb(const std::string& parameters,
			    std::unordered_map<unsigned long, Histogram>& hbosstats);


    /**
     * @brief Update the internal histogram with those included in the input map
     * @param[in] hbosstats The map between global function index and histogram
     */
    void update(const std::unordered_map<unsigned long, Histogram>& hbosstats);

    /**
     * @brief Update the internal Histogram with those included in another HbosParam instance.
     * @param[in] other The other HbosParam instance
     */
    void update(const HbosParam& other) { update(other.m_hbosstats); }

    /**
     * @brief Update the internal histogram with those included in the input map. Input map is then updated to reflect new state.
     * @param[in,out] hbosstats The map between global function index and statistics
     */
    void update_and_return(std::unordered_map<unsigned long, Histogram>& hbosstats);

    /**
     * @brief Update the internal histogram with those included in another HbosParam instance. Other HbosParam is then updated to reflect new state.
     * @param[in,out] other The other HbosParam instance
     */
    void update_and_return(HbosParam& other) { update_and_return(other.m_hbosstats); }


    nlohmann::json get_algorithm_params(const unsigned long func_id) const override;

  private:
    std::unordered_map<unsigned long, Histogram> m_hbosstats;
  };


} // end of chimbuko namespace
