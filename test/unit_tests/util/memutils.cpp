#include <malloc.h>
#include <sstream>
#include "chimbuko/util/memutils.hpp"
#include "gtest/gtest.h"


using namespace chimbuko;

TEST(TestMemory, runTests){

  //Disable heap from holding onto memory
  mallopt(M_MMAP_THRESHOLD,0);

  size_t total, resident;

  getMemUsage(total, resident);  
  std::cout << total << " " << resident  << std::endl;
  
  size_t to_alloc = 1024*1024*sizeof(double);

  std::cout << "Allocating " << to_alloc / 1024 << " kB" << std::endl;
  
  double* d = (double*)malloc(to_alloc);
  for(size_t i=0;i<1024*1024;i++)
    d[i] = i;

  size_t total2, resident2;
  getMemUsage(total2, resident2);  
  std::cout << total2 << " " << resident2 << std::endl;

  size_t delta = resident2-resident;
  
  std::cout << "Data changed by " << delta << " kB" << std::endl;

  EXPECT_NEAR(delta, to_alloc/1024, 100);


  std::cout << "Deallocating " << to_alloc / 1024 << " kB" << std::endl;
  free(d);

  size_t total3, resident3;
  getMemUsage(total3, resident3);  
  std::cout << total3 << " " << resident3 << std::endl;

  delta = resident2 - resident3;
  std::cout << "Data changed by " << delta << " kB" << std::endl;

  //It's not very accurate, or possibly the stack is growing
  EXPECT_NEAR(delta, to_alloc/1024, 200);

}
