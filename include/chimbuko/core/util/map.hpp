#pragma once
#include <chimbuko_config.h>
#include<unordered_map>

namespace chimbuko{

  /**
   * @brief Define a mapping of process/rank/thread to a data type
   */
  template<typename T>
  using mapPRT = std::unordered_map<unsigned long, std::unordered_map<unsigned long, std::unordered_map<unsigned long, T> > >;

  /**
   * @brief Get an element from the commonly-occuring triple-depth map of process/rank/thread to element (non-const)
   * @param pid The process index
   * @param rid The rank index
   * @param tid The thread index
   * @param map The map
   * @return A pointer to the element if it exists, nullptr otherwise
   */
  template<typename T>
  T* getElemPRT(const unsigned long pid, const unsigned long rid, const unsigned long tid,
		std::unordered_map<unsigned long, std::unordered_map<unsigned long, std::unordered_map<unsigned long, T> > > &map){
    auto pit = map.find(pid);
    if(pit == map.end()) return nullptr;
    auto rit = pit->second.find(rid);
    if(rit == pit->second.end()) return nullptr;
    auto tit = rit->second.find(tid);
    if(tit == rit->second.end()) return nullptr;
    return &tit->second;
  }

  /**
   * @brief Get an element from the commonly-occuring triple-depth map of process/rank/thread to element (const)
   * @param pid The process index
   * @param rid The rank index
   * @param tid The thread index
   * @param map The map
   * @return A pointer to the element if it exists, nullptr otherwise
   */
  template<typename T>
  T const* getElemPRT(const unsigned long pid, const unsigned long rid, const unsigned long tid,
		      const std::unordered_map<unsigned long, std::unordered_map<unsigned long, std::unordered_map<unsigned long, T> > > &map){
    auto pit = map.find(pid);
    if(pit == map.end()) return nullptr;
    auto rit = pit->second.find(rid);
    if(rit == pit->second.end()) return nullptr;
    auto tit = rit->second.find(tid);
    if(tit == rit->second.end()) return nullptr;
    return &tit->second;
  }


  /**
   * @brief Get the map between thread and element from the commonly-occuring triple-depth map of process/rank/thread to element (non-const)
   * @param pid The process index
   * @param rid The rank index
   * @param map The map
   * @return A pointer to the map element if it exists, nullptr otherwise
   */
  template<typename T>
  std::unordered_map<unsigned long, T>* getMapPR(const unsigned long pid, const unsigned long rid,
						 std::unordered_map<unsigned long, std::unordered_map<unsigned long, std::unordered_map<unsigned long, T> > > &map){
    auto pit = map.find(pid);
    if(pit == map.end()) return nullptr;
    auto rit = pit->second.find(rid);
    if(rit == pit->second.end()) return nullptr;
    return &rit->second;
  }

  /**
   * @brief Get the map between thread and element from the commonly-occuring triple-depth map of process/rank/thread to element (const)
   * @param pid The process index
   * @param rid The rank index
   * @param map The map
   * @return A pointer to the map element if it exists, nullptr otherwise
   */
  template<typename T>
  std::unordered_map<unsigned long, T> const* getMapPR(const unsigned long pid, const unsigned long rid,
						       const std::unordered_map<unsigned long, std::unordered_map<unsigned long, std::unordered_map<unsigned long, T> > > &map){
    auto pit = map.find(pid);
    if(pit == map.end()) return nullptr;
    auto rit = pit->second.find(rid);
    if(rit == pit->second.end()) return nullptr;
    return &rit->second;
  }

  /**
   * @brief Implementation of recursive += for unordered map
   */
  template<typename T>
  struct _plus_equals{
    static void doit(T &into, const T &from){
      into += from;
    }
  };

  template<typename KeyType, typename ValueType>
  struct _plus_equals<std::unordered_map<KeyType,ValueType> >{
    static void doit(std::unordered_map<KeyType,ValueType> &into, const std::unordered_map<KeyType,ValueType> &from){
      for(auto rit = from.begin(); rit != from.end(); ++rit){
	auto const &r_key = rit->first;
	auto const &r_val = rit->second;

	auto lit = into.find(r_key);
	//If rid key is not present in current object, just copy the data
	if(lit == into.end()){
	  into[r_key] = r_val;
	}else{
	  auto &l_val = lit->second;
	  _plus_equals<ValueType>::doit(l_val,r_val);
	}
      }
    }
  };

  /**
   * @brief For nested std::unordered_map instances, recursively merge using operator+= for the underlying elements, copying data where appropriate for keys not present in the output
   */
  template<typename T>
  void unordered_map_plus_equals(T &into, const T &from){
    _plus_equals<T>::doit(into,from);
  }


}
