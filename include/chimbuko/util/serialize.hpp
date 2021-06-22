#pragma once

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

namespace chimbuko{

  template<typename T>
  std::string cereal_serialize(const T &v){
    std::stringstream ss;
    {    
      cereal::PortableBinaryOutputArchive wr(ss);
      wr(v);    
    }
    return ss.str();
  }

  template<typename T>
  void cereal_deserialize(T &v, const std::string &sv){
    std::stringstream ss; ss << sv;;
    {    
      cereal::PortableBinaryInputArchive rd(ss);
      rd(v);    
    }
  }

};
