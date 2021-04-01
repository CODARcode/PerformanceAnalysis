#include<chimbuko/ad/utils.hpp>
#include<unordered_map>
#include "gtest/gtest.h"

using namespace chimbuko;

TEST(ADutils, testHash){
  std::unordered_map<eventID, int> map;
  std::vector<eventID> events = { eventID(0,1,2), eventID(2,1,2), eventID(2,2,2), eventID(2,1,3), eventID::root() };
  std::vector<int> vals = { 99, 108, 328 , 444, 999 };

  for(int i=0;i<events.size();i++) map[ events[i] ] = vals[i];

  EXPECT_EQ( events[4].isRoot(), true );

  for(int i=0;i<events.size();i++){
    auto it = map.find(events[i]);
    EXPECT_NE( it, map.end() );
    EXPECT_EQ( it->second, vals[i] );
  }

  {
    auto it = map.find(eventID::root());
    EXPECT_NE( it, map.end() );
    EXPECT_EQ( it->second, vals[4] );
  }

}
