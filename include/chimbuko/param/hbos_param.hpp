#pragma once
#include <chimbuko_config.h>
#include "chimbuko/param.hpp"
#include "chimbuko/util/RunStats.hpp"
#include "chimbuko/util/Histogram.hpp"
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <vector>
#include <iostream>

namespace chimbuko {

  /**
   * @@brief Implementation of ParamInterface for HBOS based anomaly detection
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
    const std::unordered_map<unsigned long, Histogram> & get_hbosstats() const{ return m_hbosstats; }

    /**
     * @brief Set the internal map between function index and statistics to the provided input
     */
    void set_hbosstats(const std::unordered_map<unsigned long, Histogram> &to){ m_hbosstats = to; }

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
    Histogram& operator [](unsigned long id) { return m_hbosstats[id]; }

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
     * @brief Get the parameters (histogram) associated with a specific function in JSON format
     */
    nlohmann::json get_algorithm_params(const unsigned long func_id) const override;

    /**
     * @brief Get the algorithm parameters for all functions. 
     * @param func_id_map A map of function index -> (program index, function name) used to populate fields in the output
     */
    nlohmann::json get_algorithm_params(const std::unordered_map<unsigned long, std::pair<unsigned long, std::string> > & func_id_map) const override;

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
     * @param hbos_threshold The threshold used for this function
     */
    void generate_histogram(const unsigned long func_id, const std::vector<double> &runtimes, double hbos_threshold, HbosParam const *global_param = nullptr);
   
  private:
    std::unordered_map<unsigned long, Histogram> m_hbosstats; /**< Map of func_id and corresponding Histogram*/
    int m_maxbins; /**< Maximum number of bins to use in the histograms */
  };


} // end of chimbuko namespace
