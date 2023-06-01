#pragma once
#include <chimbuko_config.h>
#include <chimbuko/util/error.hpp>
#include<cstdlib>
#include<vector>

namespace chimbuko{

  //A local (not thread safe) fast pool reallocator
  template<typename T>
  class chunkAllocator{
  protected:
    std::vector<T*> m_chunks;
    std::vector<T*> m_reuse;
    size_t m_chunk_count;
    size_t m_off;
    void allocChunk(){
      m_chunks.push_back( (T*)malloc(m_chunk_count*sizeof(T)) );
    }
  public:
    T* get(){
      if(m_reuse.size()){ //reuse existing pointer if available
	T *ret = m_reuse.back();
	m_reuse.pop_back();
	return ret;
      }else if(m_off == m_chunk_count){ //no space left in chunk, allocate a new one
	allocChunk();
	m_off = 1;
	return m_chunks.back();
      }else{
	++m_off;
	return m_chunks.back()+m_off-1;
      }
    }
    void free(T* p){
      m_reuse.push_back(p);
    }

    chunkAllocator(size_t chunk_count = 100): m_chunk_count(chunk_count), m_off(0){
      allocChunk();
    }

    ~chunkAllocator(){
      for(T* p: m_chunks)
	::free(p);
    }
  };

}
