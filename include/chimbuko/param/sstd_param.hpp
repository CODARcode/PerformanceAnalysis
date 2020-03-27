#pragma once

#include "chimbuko/param.hpp"
#include "chimbuko/util/RunStats.hpp"
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace chimbuko {

  class SstdParam : public ParamInterface
  {
  public:
    SstdParam();
    SstdParam(const std::vector<int>& n_ranks);
    ~SstdParam();
    void clear() override;

    size_t size() const override { return m_runstats.size(); }

    /**
     * @brief Convert internal run statistics to string format for IO
     * @return Run statistics in string format
     */
    std::string serialize() const override;

    /**
     * @brief Update the internal run statistics with those included in the serialized input map
     * @param runstats The serialized input map
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
     * @brief Get an element of the internal map
     */
    RunStats& operator [](unsigned long id) { return m_runstats[id]; }

    /**
     * @brief Get the internal map
     */
    const std::unordered_map<unsigned long, RunStats> & get_runstats() const{ return m_runstats; }
    
  private:
    std::unordered_map<unsigned long, RunStats> m_runstats;
  };

} // end of chimbuko namespace
