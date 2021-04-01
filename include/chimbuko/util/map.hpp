#pragma once

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



}
