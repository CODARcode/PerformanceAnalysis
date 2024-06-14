#pragma once
#include <chimbuko_config.h>
#include "chimbuko/core/param.hpp"
#include "chimbuko/core/util/RunStats.hpp"
#include "chimbuko/core/util/Histogram.hpp"
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <vector>
#include <iostream>

namespace chimbuko {

  /**
   * @brief The algorithm parameters for a given function
   */
  class HbosFuncParam{
  public:
    HbosFuncParam();      

    const Histogram &getHistogram() const{ return m_histogram; }
    Histogram &getHistogram(){ return m_histogram; }
    
    double getInternalGlobalThreshold() const{ return m_internal_global_threshold; }
    void setInternalGlobalThreshold(double to){ m_internal_global_threshold = to; }

    /**
     * @brief Merge another instance of HbosFuncParam into this one
     * @param other The other instance
     * @param bw The specifier for the bin width used in the histogram merge
     */
    void merge(const HbosFuncParam &other, const binWidthSpecifier &bw);

    nlohmann::json get_json() const;

    /**
     * @brief Serialize using cereal
     */
    template<class Archive>
    void serialize(Archive & archive){
      archive(m_histogram, m_internal_global_threshold);
    }

    bool operator==(const HbosFuncParam &other) const{ return m_histogram == other.m_histogram && m_internal_global_threshold == other.m_internal_global_threshold; }
    bool operator!=(const HbosFuncParam &other) const{ return !(*this == other); }

  private:
    Histogram m_histogram; /**< The function runtime histogram */
    double m_internal_global_threshold; /**< An internal global threshold used by the algorithm*/
  };


  /**
   * @brief Implementation of ParamInterface for HBOS based anomaly detection
   */
  class HbosParam : public ParamInterface {
  public:
    HbosParam();
    ~HbosParam();

    /**
     * @brief Copy an existing HbosParam
     *
     * Thread safe
     */
    void copy(const HbosParam &r);

    /**
     * @brief Clear all statistics
     */
    void clear() override;

    /**
     * @bin Check if the statistics for a function exist in the histogram
     */
    bool find(const unsigned long func_id) const;

    /**
     * @brief Get the internal map between global function index and statistics
     */
    const std::unordered_map<unsigned long, HbosFuncParam> & get_hbosstats() const{ return m_hbosstats; }

    /**
     * @brief Set the internal map between function index and statistics to the provided input
     */
    void set_hbosstats(const std::unordered_map<unsigned long, HbosFuncParam> &to){ m_hbosstats = to; }

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
     * @param parameters The serialized input data
     */
    void assign(const std::string& parameters) override;

    void show(std::ostream& os) const override;

    /**
     * @brief Set the internal data to match those included in the input. Overwrite performed only for those keys in input.
     * @param params The input data
     */
    void assign(const HbosParam & params);

    /**
     * @brief Convert a HbosParam into a string representration
     */
    static std::string serialize_cerealpb(const HbosParam &params);

    /**
     * @brief Get an element of the internal map
     * @param id The global function index
     */
    HbosFuncParam& operator [](unsigned long id) { return m_hbosstats[id]; }

    /**
     * @brief Deserialize a serialized HbosParam object Histogram Cereal portable binary representation string into a map
     * @param[in] parameters The serialized object
     * @param[out] into The deserialized object
     */
    static void deserialize_cerealpb(const std::string& parameters, HbosParam &into);

    /**
     * @brief Update the internal histogram with those included in the input data
     * @param[in] param The input HbosParam object
     *
     * The other instance is locked during the process
     */
    void update(const HbosParam &other);

    /**
     * @brief Update the internal run statistics with those from another instance
     *
     * The instance will be dynamically cast to the derived type internally, and will throw an error if the types do not match
     */
    void update(const ParamInterface &other) override{ update(dynamic_cast<HbosParam const&>(other)); }

    /**
     * @brief Update the internal data with those included in the input. The input is then updated to reflect new state.
     * @param[in,out] from_into The input/output data
     */
    void update_and_return(HbosParam &from_into);

    /**
     * @brief Get the parameters (histogram) associated with a specific model index in JSON format
     */
    nlohmann::json get_algorithm_params(const unsigned long model_idx) const override;

    /**
     * @brief Get the algorithm parameters for all model indices. Returns a map of model index to JSON-formatted parameters. Parameter format is algorithm dependent
     */
    std::unordered_map<unsigned long, nlohmann::json> get_all_algorithm_params() const override;

    /**
     * @brief Get the maximum number of bins
     */
    inline int getMaxBins() const{ return m_maxbins; }

    /**
     * @brief Set the maximum number of bins
     */
    inline void setMaxBins(const int b){ m_maxbins = b; }

    /**
     * @brief Generate the histogram for a particular function based on the batch of runtimes
     * @param func_id The function index
     * @param runtimes The function runtimes
     * @param global_param A pointer to the current global histogram. If non-null both the global model and the runtimes dataset will be used to determine the optimal bin width
     * @param global_threshold_init The initial value of the internal, global threshold
     */
    void generate_histogram(const unsigned long func_id, const std::vector<double> &runtimes, double global_threshold_init,  HbosParam const *global_param = nullptr);

    /**
     * @brief Get the algorithm parameters in JSON form
     */
    nlohmann::json get_json() const override;
    
    /**
     * @brief Set the algorithm parameters from the input JSON structure
     */
    void set_json(const nlohmann::json &from) override;

    bool operator==(const HbosParam &r) const;
    inline bool operator!=(const HbosParam &r) const{ return !(*this == r); }
   
  private:
    /**
     * @brief Common functionality between update and update_and_return; assumed a mutex lock exists on this and other
     */
    void update_internal(const HbosParam &other);
    
    std::unordered_map<unsigned long, HbosFuncParam> m_hbosstats; /**< Map of func_id and corresponding Histogram*/
    int m_maxbins; /**< Maximum number of bins to use in the histograms */
  };


} // end of chimbuko namespace
