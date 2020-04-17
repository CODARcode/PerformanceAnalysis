#pragma once

#include "chimbuko/param.hpp"
#include "chimbuko/util/RunStats.hpp"
#include <unordered_map>
#include <nlohmann/json.hpp>

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
     * @brief Convert a run statistics mapping into a string
     * @param The run stats mapping
     * @return Run statistics in string format
     */
    static std::string serialize(const std::unordered_map<unsigned long, RunStats>& runstats);

    /**
     * @brief Convert a run statistics string into a map
     * @param[in] parameters The parameter string
     * @param[out] runstats The run stats map
     */
    static void deserialize(const std::string& parameters,
			    std::unordered_map<unsigned long, RunStats>& runstats);


    /**
     * @brief Update the internal run statistics with those included in the input map
     * @param[in] runstats The input map
     */
    void update(const std::unordered_map<unsigned long, RunStats>& runstats);

    /**
     * @brief Update the internal statistics with those included in another SstdParam instance.
     * @param[in] other The other SstdParam instance
     */
    void update(const SstdParam& other) { update(other.m_runstats); }
    
    /**
     * @brief Update the internal run statistics with those included in the input map. Input map is then updated to reflect new state.
     * @param[in,out] runstats The input/output map
     */
    void update_and_return(std::unordered_map<unsigned long, RunStats>& runstats);

    /**
     * @brief Update the internal statistics with those included in another SstdParam instance. Other SstdParam is then updated to reflect new state.
     * @param[in,out] other The other SstdParam instance
     */
    void update_and_return(SstdParam& other) { update_and_return(other.m_runstats); }

    /**
     * @brief Set the internal run statistics to match those included in the input map. Overwrite performed only for those keys in input.
     * @param runstats The input map
     */
    void assign(const std::unordered_map<unsigned long, RunStats>& runstats);

    /**
     * @brief Get an element of the internal map. id is the function index
     */
    RunStats& operator [](unsigned long id) { return m_runstats[id]; }

    /**
     * @brief Get the internal map
     */
    const std::unordered_map<unsigned long, RunStats> & get_runstats() const{ return m_runstats; }

    /**
     * @brief Get the statistical distribution associated with a given function
     */
    const RunStats & get_function_stats(const unsigned long func_id) const override;

    
  private:
    std::unordered_map<unsigned long, RunStats> m_runstats; /**< Map of function index to statistics*/
  };

} // end of chimbuko namespace
