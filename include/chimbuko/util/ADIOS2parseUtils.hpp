#pragma once

#include <mpi.h>
#include <adios2.h>
#include <string>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <set>
#include <regex>
#include <chimbuko/util/string.hpp>

namespace chimbuko{

/**
 * @brief Wrapper allowing ostream output of a string map object
 */
struct mapPrint{
  const std::map<std::string, std::string> &mp;
  mapPrint(const std::map<std::string, std::string> &mp): mp(mp){}
};
/**
 * @brief ostream output of a map using mapPrint wrapper
 */
std::ostream & operator<<(std::ostream &os, const mapPrint &mp);

/**
 * @brief Wrapper allowing ostream output of a vector object
 */
template<typename T>
struct vecPrint{
  const std::vector<T> &mp;
  vecPrint(const std::vector<T> &mp): mp(mp){}
};
/**
 * @brief ostream output of a vector using vecPrint wrapper
 */
template<typename T>
std::ostream & operator<<(std::ostream &os, const vecPrint<T> &mp){
  os << "(";
  for(auto it=mp.mp.begin(); it != mp.mp.end(); ++it)
    os << *it << " ";
  os << ")";
  return os;
}


/**
 * @brief Abstract interface for an object that reads, stores and outputs data or arrays of data from ADIOS2 streams
 */
struct varBase{
  std::string name;

  /**
   * @brief Construct object with variable name 'name'
   */
  varBase(const std::string &name): name(name){}
  
  /**
   * @brief Get the value as a human-readable string
   */
  virtual std::string value() const{ return ""; };

  /**
   * @brief Read the variable from the ADIOS2 stream
   */
  virtual void get(adios2::IO &io, adios2::Engine &eng){
    assert(0);
  };

  /**
   * @brief Write the variable to the ADIOS2 stream
   */
  virtual void put(adios2::IO &io, adios2::Engine &eng){
    assert(0);
  }
  virtual ~varBase(){}
};

/**
   @brief Capture POD (single-value) data
*/
template<typename T>
class varPOD: public varBase{
  T val;
public:

  varPOD(const std::string &name): varBase(name){}
  varPOD(const std::string &name, adios2::IO &io, adios2::Engine &eng): varBase(name){
    this->get(io, eng);
  }
  
  void get(adios2::IO &io, adios2::Engine &eng){
    auto var = io.InquireVariable<T>(this->name);
    if(!var){
      throw std::runtime_error("Variable " + this->name + " does not seem to exist?!");
    }else{
      eng.Get<T>(var, &val, adios2::Mode::Sync);
    }
  }
  virtual void put(adios2::IO &io, adios2::Engine &eng){
    adios2::Variable<T> var;
    if(!(var = io.InquireVariable<T>(this->name)))
      var = io.DefineVariable<T>(this->name);
    eng.Put<T>(var, &val,  adios2::Mode::Sync);
  }
  std::string value() const{
    std::ostringstream os;
    os <<val;
    return os.str();
  }
};

/**
 * @brief Capture multi-dimensional tensor data
 */
template<typename T>
class varTensor: public varBase{
  std::vector<unsigned long> shape; /**< The "shape" of the tensor */

  /**
   * @brief Compute the lexicographic offset for coordinate 'c' assuming row-major order
   */
  template<typename listType>
  inline size_t map(const listType &c) const{
    assert(c.size() == shape.size());
    size_t out = 0;
    auto it = c.begin();
    for(unsigned long i=0;i<shape.size();i++){  //k + nk*(j + nj*i)  row-major order
      out *= shape[i];
      out += *it;
      ++it;
    }
    return out;
  }
  /**
   * @brief Unmap an offset into a coordinate
   */
  inline void unmap(std::vector<unsigned long> &c, size_t o) const{
    c.resize(shape.size());
    for(int i=shape.size()-1;i>=0;i--){
      c[i] = o % shape[i];
      o /= shape[i];
    }    
  }

  
  std::vector<T> val;
public:


  varTensor(const std::string &name): varBase(name){}
  varTensor(const std::string &name, const std::vector<unsigned long> &shape, adios2::IO &io, adios2::Engine &eng): varBase(name), shape(shape){
    this->get(io, eng);
  }
  
  void get(adios2::IO &io, adios2::Engine &eng){
    auto var = io.InquireVariable<T>(this->name);
    if(!var){
      throw std::runtime_error("Variable " +  this->name + " does not seem to exist?!");
    }else{
      std::vector<unsigned long> zeros(shape.size(),0);
      var.SetShape(shape);
      var.SetSelection({zeros, shape});    

      size_t sz = 1; for(int i=0;i<shape.size();i++) sz *= shape[i];
      val.resize(sz);
      memset((void*)val.data(), 0, sz*sizeof(T));
      
      eng.Get<T>(var, val.data(), adios2::Mode::Sync);
    }
  }
  void put(adios2::IO &io, adios2::Engine &eng){
    std::vector<unsigned long> zeros(shape.size(),0);
    adios2::Variable<T> var;
    if(!(var = io.InquireVariable<T>(this->name)))
      var = io.DefineVariable<T>(this->name, shape, zeros, shape);
    else{
      var.SetShape(shape);
      var.SetSelection({zeros, shape});
    }      
    eng.Put<T>(var, val.data(),  adios2::Mode::Sync);
  }
  std::string value() const{
    std::ostringstream os;
    std::vector<unsigned long> c(shape.size());
    for(size_t i=0;i<val.size();i++){
      unmap(c, i);
      if( map(c) != i ) assert(0); //check consistency of map and unmap
      os << vecPrint<unsigned long>(c) << ":" << val[i] << " ";

      if(shape.size() == 2 && i % shape[1] == shape[1]-1) os << std::endl;  //endline after each row of matrix
    }
    return os.str();
  }

  /**
   * @brief Get the value at given coordinate (non-const)
   */
  inline T & operator()(const std::vector<unsigned long> &coord){
    return val[map(coord)];
  }

  /**
   * @brief Get the value at given coordinate (const)
   */
  inline const T & operator()(const std::vector<unsigned long> &coord) const{
    return val[map(coord)];
  }

  /**
   * @brief Get the shape of the tensor
   */
  const std::vector<unsigned long> & getShape() const{ return shape; }

};




/**
 * @brief A factory for generating varBase derived class instances that contain the data read from the input stream
 * 
 * Returns a NULL ptr if the type is not supported
 * The name/varinfo data can be obtained using the adios2::IO::AvailableVariables method
*/
varBase* parseVariable(const std::string &name, const std::map<std::string, std::string> &varinfo, adios2::IO &io, adios2::Engine &eng);

}
