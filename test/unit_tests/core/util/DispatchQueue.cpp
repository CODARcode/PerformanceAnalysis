#include "chimbuko/core/util/DispatchQueue.hpp"
#include "gtest/gtest.h"

using namespace chimbuko;

TEST(DispatchQueueTestConstructor, dispatchLvalueRef){
  DispatchQueue* q = new DispatchQueue("myqueue", 2);

  std::string w1;
  std::string w2;
  
  std::function<void(void)> t1 = [&](){ w1 = "hello_t1"; };
  std::function<void(void)> t2 = [&](){ w2 = "hello_t2"; };
  
  q->dispatch(t1);
  q->dispatch(t2);

  delete q; //return waits for completion

  std::cout << w1 << std::endl;
  std::cout << w2 << std::endl;
  
  EXPECT_EQ(w1, std::string("hello_t1") );
  EXPECT_EQ(w2, std::string("hello_t2") );
}
TEST(DispatchQueueTestConstructor, dispatchRvalueRef){
  DispatchQueue* q = new DispatchQueue("myqueue", 2);

  std::string w1;
  std::string w2;
  
  std::function<void(void)> t1 = [&](){ w1 = "hello_t1"; };
  std::function<void(void)> t2 = [&](){ w2 = "hello_t2"; };
  
  q->dispatch(std::move(t1));
  q->dispatch(std::move(t2));

  delete q; //return waits for completion

  std::cout << w1 << std::endl;
  std::cout << w2 << std::endl;
  
  EXPECT_EQ(w1, std::string("hello_t1") );
  EXPECT_EQ(w2, std::string("hello_t2") );
}
