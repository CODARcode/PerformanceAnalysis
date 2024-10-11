#pragma once
#include <chimbuko_config.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include "chimbuko/core/util/RunStats.hpp"

namespace chimbuko {

  /**
   * @brief A class containing a map of a string to its aggregated statistics, used for performance logging
   */
  class RunMetric {
  public:
    RunMetric(){ }
    ~RunMetric(){ }

    /**
     * @brief Add a value to the statistics tagged by the provided name
     *
     * A new entry in the map is created if the name has not been provided previously
     */
    void add(std::string name, double val){
      if (m_metrics.count(name) == 0){
	m_metrics[name].push(0);
	m_metrics[name].clear();
	m_metrics[name].set_do_accumulate(true);            
      }
      m_metrics[name].push(val);
      m_last[name] = val;
    }

    /**
     * @brief Write the data to disk in JSON form
     */
    void dump(std::string path, std::string filename="metric.json") const{
      std::ofstream f;
      f.open(path + "/" + filename);
        
      if (f.is_open() && m_metrics.size()){
	nlohmann::json j;
	for (auto m: m_metrics){
	  j[m.first] = m.second.get_json();
	}
	f << j.dump(2) << std::endl;
      }

      f.close();
    }

    /**
     * @brief Write the data to a stream
     */
    void dump(std::ostream &os) const{
      if (m_metrics.size()){
	nlohmann::json j;
	for (auto m: m_metrics){
	  j[m.first] = m.second.get_json();
	}
	os << j.dump(2) << std::endl;
      }
    }


    /**
     * @brief Combine this instance with another
     * Does not perform any action on the last recorded value
     */
    RunMetric & operator+=(const RunMetric &r){
      for(auto const &e : r.m_metrics){
	auto it = m_metrics.find(e.first);
	if(it == m_metrics.end()) m_metrics[e.first] = e.second;
	else it->second += e.second;
      }
      return *this;
    }

    /**
     * @brief Get the statistics for a particular metric. Returns nullptr if the tag does not exist
     */
    RunStats const* getMetric(const std::string &tag) const{
      auto it = m_metrics.find(tag);
      if(it == m_metrics.end()) return nullptr;
      else return &it->second;
    }
    
    /**
     * @brief Get the last recorded value for a particular metric
     * @return A bool,double pair where the first entry indicates whether the tag exists, and the second its value
     */
    std::pair<bool,double> getLastRecorded(const std::string &tag) const{
      auto it = m_last.find(tag);
      if(it == m_last.end()) return std::pair<bool,double>(false,0);
      else return std::pair<bool,double>(true,it->second);
    }
     
    /**
     * @brief Clear the state
     */
    void clear(){
      m_metrics.clear();
      m_last.clear();
    }
    
  private:
    std::unordered_map<std::string, RunStats> m_metrics; /**< Map of tag to statistics object */
    std::unordered_map<std::string, double> m_last; /**< Map of tag to last recorded value*/
  };

};
