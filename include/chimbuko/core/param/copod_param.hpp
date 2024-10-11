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
   * @brief The algorithm parameters for a given model
   */
  class CopodFuncParam{
  public:
    CopodFuncParam();      

    const Histogram &getHistogram() const{ return m_histogram; }
    Histogram &getHistogram(){ return m_histogram; }

    double getInternalGlobalThreshold() const{ return m_internal_global_threshold; }
    void setInternalGlobalThreshold(double to){ m_internal_global_threshold = to; }
    
    /**
     * @brief Merge another instance of HbosFuncParam into this one
     * @param other The other instance
     */
    void merge(const CopodFuncParam &other);

    nlohmann::json get_json() const;

    /**
     * @brief Serialize using cereal
     */
    template<class Archive>
    void serialize(Archive & archive){
      archive(m_histogram, m_internal_global_threshold);
    }

    bool operator==(const CopodFuncParam &other) const{ return m_histogram == other.m_histogram && m_internal_global_threshold == other.m_internal_global_threshold; }
    bool operator!=(const CopodFuncParam &other) const{ return !(*this == other); }

  private:
    Histogram m_histogram; /**< The function runtime histogram */
    double m_internal_global_threshold; /**< An internal global threshold used by the algorithm*/
  };


  /**
   * @@brief Implementation of ParamInterface for COPOD based anomaly detection
   */
  class CopodParam : public ParamInterface {
  public:
    CopodParam();
    ~CopodParam();
    /**
     * @brief Clear all statistics
     */
    void clear() override;

    /**
     * @brief Copy an existing HbosParam
     */
    void copy(const CopodParam &r){ m_copodstats = r.m_copodstats; }

    /**
     * @brief Check if the statistics for a model exist in the histogram
     */
    const int find(const unsigned long model_id);

    /**
     * @brief Get the internal map between model index and statistics
     */
    const std::unordered_map<unsigned long, CopodFuncParam> & get_copodstats() const{ return m_copodstats; }

    /**
     * @brief Set the internal map between model index and statistics to the provided input
     */
    void set_copodstats(const std::unordered_map<unsigned long, CopodFuncParam> &to){ m_copodstats = to; }

    /**
     * @brief Get the number of functions for which statistics are being collected
    */
    size_t size() const override { return m_copodstats.size(); }

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
     * @brief Set the internal data to match those included in the input. Overwrite performed only for those keys in input.
     * @param params The input data
     */
    void assign(const CopodParam &other);

    /**
     * @brief Convert a CopodParam into a Cereal portable binary representration
     * @param param The CopodParam instance
     * @return Histogram in string format
     */
    static std::string serialize_cerealpb(const CopodParam &param);

    /**
     * @brief Get an element of the internal map
     * @param id The model index
     */
    CopodFuncParam& operator [](unsigned long id) { return m_copodstats[id]; }

    /**
     * @brief Deserialize a CopodParam instance
     * @param[in] parameters The parameter string
     * @param[out] p The CopodParam instance
     */
    static void deserialize_cerealpb(const std::string& parameters,  CopodParam &p);

    /**
     * @brief Update the internal Histogram with those included in another CopodParam instance.
     * @param[in] other The other CopodParam instance
     *
     * The other instance is locked during the process
     */
    void update(const CopodParam& other);

    /**
     * @brief Update the internal model from another instance
     *
     * The instance will be dynamically cast to the derived type internally, and will throw an error if the types do not match
     */
    void update(const ParamInterface &other) override{ update(dynamic_cast<CopodParam const&>(other)); }

    /**
     * @brief Update the internal histogram with those included in another CopodParam instance. Other CopodParam is then updated to reflect new state.
     * @param[in,out] other The other CopodParam instance
     */
    void update_and_return(CopodParam& other);

    /**
     * @brief Get the parameters (histogram) associated with a specific model index in JSON format
     */
    nlohmann::json get_algorithm_params(const unsigned long model_idx) const override;

    /**
     * @brief Get the algorithm parameters for all model indices. Returns a map of model index to JSON-formatted parameters. Parameter format is algorithm dependent
     */
    std::unordered_map<unsigned long, nlohmann::json> get_all_algorithm_params() const override;

    /**
     * @brief Serialize the set of algorithm parameters in JSON form for purpose of inter-run persistence, format may differ from the above
     */
    nlohmann::json serialize_json() const override;

    /**
     * @brief De-serialize the set of algorithm parameters from JSON form created by serialize_json
     */
    void deserialize_json(const nlohmann::json &from) override;

  private:
    std::unordered_map<unsigned long, CopodFuncParam> m_copodstats; /**< Map of model index and corresponding Histogram*/
  };


} // end of chimbuko namespace
