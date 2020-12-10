#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
#include "chimbuko/util/RunStats.hpp"

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
     * @brief Combine this instance with another
     */
    RunMetric & operator+=(const RunMetric &r){
      for(auto const &e : r.m_metrics){
	auto it = m_metrics.find(e.first);
	if(it == m_metrics.end()) m_metrics[e.first] = e.second;
	else it->second += e.second;
      }
      return *this;
    }
    
  private:
    std::unordered_map<std::string, RunStats> m_metrics; /**< Map of tag to statistics object */
  };

};
