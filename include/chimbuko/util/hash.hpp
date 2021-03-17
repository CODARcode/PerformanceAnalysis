#pragma once

#include<array>

namespace chimbuko{

  /**
   * @brief Hash function for std::array
   */
  template<typename T, size_t N>
  struct ArrayHasher {
    std::size_t operator()(const std::array<T, N>& a) const{
      std::size_t h = 0;
      
      for (auto e : a) {
	h ^= std::hash<T>{}(e)  + 0x9e3779b9 + (h << 6) + (h >> 2); 
      }
      return h;
    }   
  };

};
