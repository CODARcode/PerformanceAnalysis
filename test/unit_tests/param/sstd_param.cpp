#include "chimbuko/param/sstd_param.hpp"
#include "gtest/gtest.h"
#include <sstream>

using namespace chimbuko;

struct SstdParamAcc: public SstdParam{
  std::unordered_map<unsigned long, RunStats> & access_runstats(){ return this->SstdParam::access_runstats(); }    
};

TEST(TestSstdParam, serialize){
  SstdParam param;
  int nfunc = 100;
  for(int i=0;i<nfunc;i++){
    RunStats &stats = param[i];
    for(int s=0;s<100;s++)
      stats.push(s);
  }

  std::string ser = SstdParam::serialize_cerealpb(param.get_runstats());
  std::cout << "Binary length: " << ser.size() << std::endl;
  
  SstdParamAcc rparam;
  SstdParam::deserialize_cerealpb(ser, rparam.access_runstats());
  
  EXPECT_EQ( param.get_runstats().size(), rparam.get_runstats().size() );
  for(auto const &e: param.get_runstats()){
    auto it = rparam.get_runstats().find(e.first);
    EXPECT_NE(it, rparam.get_runstats().end());
    EXPECT_TRUE(e.second.equiv(it->second));
  }    

  

}
