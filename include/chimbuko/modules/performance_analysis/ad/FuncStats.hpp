#pragma once
#include <chimbuko_config.h>
#include <chimbuko/core/util/RunStats.hpp>

namespace chimbuko{
  namespace modules{
    namespace performance_analysis{

      /**
       * @brief Structure to store the profile statistics associated with a specific function
       */
      struct FuncStats{
	unsigned long pid; /**< Program index*/
	unsigned long id; /**< Function index*/
	std::string name; /**< Function name*/
	unsigned long n_anomaly; /**< Number of anomalies*/
	RunStats inclusive; /**< Inclusive runtime stats*/
	RunStats exclusive; /**< Exclusive runtime stats*/

	FuncStats(): n_anomaly(0){}
      
	/**
	 * @brief Create a FuncStats instance of a particular pid, id, name
	 */
	FuncStats(const unsigned long pid, const unsigned long id, const std::string &name): pid(pid), id(id), name(name), n_anomaly(0){}
                
	/**
	 * @brief Create a JSON object from this instance
	 */
	nlohmann::json get_json() const;

	/**
	 * @brief Equivalence operator
	 */
	bool operator==(const FuncStats &r) const{
	  return pid==r.pid && id==r.id && name==r.name && n_anomaly==r.n_anomaly && inclusive==r.inclusive && exclusive==r.exclusive;
	}

	/**
	 * @brief Serialize using cereal
	 */
	template<class Archive>
	void serialize(Archive & archive){
	  archive(pid,id,name,n_anomaly,inclusive,exclusive);
	}

	/**
	 * @brief Inequalityoperator
	 */
	inline bool operator!=(const FuncStats &r) const{ return !(*this == r); }
      };

    }
  }
}
