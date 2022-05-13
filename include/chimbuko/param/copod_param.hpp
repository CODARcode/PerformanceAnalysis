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

    const int find(const unsigned long& func_id);

    /**
     * @brief Get the internal map between global function index and statistics
     */
    const std::unordered_map<unsigned long, Histogram> & get_hbosstats() const{ return m_copodstats; }

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
     * @brief Set the internal Histogram to match those included in the input map. Overwrite performed only for those keys in input.
     * @param copodstats The input map between global function index and Histogram
     */
    void assign(const std::unordered_map<unsigned long, Histogram>& copodstats);

    /**
     * @brief Convert a Histogram mapping into a Cereal portable binary representration
     * @param copodstats The Histogram mapping
     * @return Histogram in string format
     */
    static std::string serialize_cerealpb(const std::unordered_map<unsigned long, Histogram>& copodstats);

    /**
     * @brief Get an element of the internal map
     * @param id The global function index
     */
    Histogram& operator [](unsigned long id) { return m_copodstats[id]; }

    /**
     * @brief Convert a Histogram Cereal portable binary representation string into a map
     * @param[in] parameters The parameter string
     * @param[out] copodstats The map between global function index and histogram statistics
     */
    static void deserialize_cerealpb(const std::string& parameters,
			    std::unordered_map<unsigned long, Histogram>& copodstats);


    /**
     * @brief Update the internal histogram with those included in the input map
     * @param[in] copodstats The map between global function index and histogram
     */
    void update(const std::unordered_map<unsigned long, Histogram>& copodstats);

    /**
     * @brief Update the internal Histogram with those included in another CopodParam instance.
     * @param[in] other The other CopodParam instance
     *
     * The other instance is locked during the process
     */
    void update(const CopodParam& other);

    /**
     * @brief Update the internal run statistics with those from another instance
     *
     * The instance will be dynamically cast to the derived type internally, and will throw an error if the types do not match
     */
    void update(const ParamInterface &other) override{ update(dynamic_cast<CopodParam const&>(other)); }

    /**
     * @brief Update the internal histogram with those included in the input map. Input map is then updated to reflect new state.
     * @param[in,out] copodstats The map between global function index and statistics
     */
    void update_and_return(std::unordered_map<unsigned long, Histogram>& copodstats);

    /**
     * @brief Update the internal histogram with those included in another CopodParam instance. Other CopodParam is then updated to reflect new state.
     * @param[in,out] other The other CopodParam instance
     */
    void update_and_return(CopodParam& other) { update_and_return(other.m_copodstats); }


    nlohmann::json get_algorithm_params(const unsigned long func_id) const override;

    /**
     * @brief Get the algorithm parameters for all functions. 
     * @param func_id_map A map of function index -> (program index, function name) used to populate fields in the output
     */
    nlohmann::json get_algorithm_params(const std::unordered_map<unsigned long, std::pair<unsigned long, std::string> > & func_id_map) const override;

  private:
    std::unordered_map<unsigned long, Histogram> m_copodstats; /**< Map of func_id and corresponding Histogram*/
  };


} // end of chimbuko namespace
