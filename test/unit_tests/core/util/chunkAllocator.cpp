#include<chimbuko/core/util/chunkAllocator.hpp>
#include "gtest/gtest.h"
#include <sstream>
#include "../../unit_test_common.hpp"

using namespace chimbuko;


template<typename T>
class testChunkAllocator: public chunkAllocator<T>{
public:
  testChunkAllocator(size_t chunk_count): chunkAllocator<T>(chunk_count){}
  
  const std::vector<T*> & get_m_chunks(){ return this->m_chunks; }
  const std::vector<T*> & get_m_reuse(){ return this->m_reuse; }
  size_t get_m_off(){ return this->m_off; }
  size_t get_m_chunk_count(){ return this->m_chunk_count; }
};


TEST(TestChunkAllocator, testIt){
  testChunkAllocator<double> c(2);
  EXPECT_EQ(c.get_m_chunks().size(),1);
  EXPECT_EQ(c.get_m_chunk_count(),2);
  
  double* p = c.get();
  double* p1 = p;
  EXPECT_EQ(c.get_m_off(),1);
  EXPECT_EQ(p, c.get_m_chunks()[0]);

  p = c.get();
  EXPECT_EQ(p, c.get_m_chunks()[0]+1);
  EXPECT_EQ(c.get_m_off(),2);

  p = c.get();
  EXPECT_EQ(c.get_m_chunks().size(),2);
  EXPECT_EQ(c.get_m_off(),1);
  EXPECT_EQ(p, c.get_m_chunks()[1]);

  p = c.get();
  EXPECT_EQ(c.get_m_off(),2);
  EXPECT_EQ(p, c.get_m_chunks()[1]+1);

  p = c.get();
  EXPECT_EQ(c.get_m_chunks().size(),3);
  EXPECT_EQ(c.get_m_off(),1);
  EXPECT_EQ(p, c.get_m_chunks()[2]);

  //Check reuse
  c.free(p1);
  p = c.get();
  EXPECT_EQ(c.get_m_off(),1);
  EXPECT_EQ(p,p1);
}
