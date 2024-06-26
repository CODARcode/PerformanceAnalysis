#include<chimbuko_config.h>
#include<chimbuko/core/util/pointerRegistry.hpp>
#include "gtest/gtest.h"
#include "../../../unit_tests/unit_test_common.hpp"

using namespace chimbuko;



struct A{
  int idx;
  std::vector<int> &dord;

  A(int _idx, std::vector<int> &_dord): idx(_idx), dord(_dord){
  }
  
  ~A(){
    std::cout << "Erasing " << idx << std::endl;
    dord.push_back(idx);   
  }
};

TEST(TestPointerRegistry, basic){
  std::vector<int> dord;

  PointerRegistry reg;

  A* a1 = new A(1, dord);
  A* a2 = new A(2, dord);
  A* a3 = new A(3, dord);

  reg.registerPointer(a1);
  reg.registerPointer(a2);
  reg.registerPointer(a3);
  
  std::cout << "This should erase idx 3,2,1" << std::endl;
  reg.resetPointers();
  
  EXPECT_EQ(a1,nullptr);
  EXPECT_EQ(a2,nullptr);
  EXPECT_EQ(a3,nullptr);
  
  EXPECT_EQ(dord.size(),3);  
  EXPECT_EQ(dord[0],3);
  EXPECT_EQ(dord[1],2);
  EXPECT_EQ(dord[2],1); 

  //Test it doesn't fail if the pointers are null
  reg.resetPointers();


  //Test we can deregister
  reg.deregisterPointers();  
  a1 = new A(1, dord);
  std::cout << "This should *not* erase idx 1" << std::endl;
  reg.resetPointers();
  
  EXPECT_NE(a1,nullptr);
  
  std::cout << "Erasing idx 1 for exit" << std::endl;
  delete a1;
}
