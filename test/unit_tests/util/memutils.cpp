#include <sstream>
#include "chimbuko/util/memutils.hpp"
#include "gtest/gtest.h"

using namespace chimbuko;

TEST(TestMemory, runTests){

  size_t total, resident, data;

  getMemUsage(total, resident, data);  
  std::cout << total << " " << resident << " " << data << std::endl;
  
  size_t to_alloc = 1024*1024*sizeof(double);

  std::cout << "Allocating " << to_alloc / 1024 << " kB" << std::endl;
  
  double* d = (double*)malloc(to_alloc);
  for(size_t i=0;i<1024*1024;i++)
    d[i] = i;

  size_t total2, resident2, data2;
  getMemUsage(total2, resident2, data2);  
  std::cout << total2 << " " << resident2 << " " << data2 << std::endl;

  size_t delta = data2-data;
  
  std::cout << "Data changed by " << delta << " kB" << std::endl;

  EXPECT_NEAR(delta, to_alloc/1024, 50);


  std::cout << "Deallocating " << to_alloc / 1024 << " kB" << std::endl;
  free(d);

  size_t total3, resident3, data3;
  getMemUsage(total3, resident3, data3);  
  std::cout << total3 << " " << resident3 << " " << data3 << std::endl;

  std::cout << "Data changed by " << delta << " kB" << std::endl;

  delta = data2 - data3;
  EXPECT_NEAR(delta, to_alloc/1024, 50);

}
