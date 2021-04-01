#include<chimbuko/ad/ADNormalEventProvenance.hpp>
#include "gtest/gtest.h"

using namespace chimbuko;

TEST(TestADNormalEventProvenance, works){
  nlohmann::json entry1 = { {"hello","world"} };
  nlohmann::json entry2 = { {"what's","up"} };
  nlohmann::json entry3 = { {"you","again"} };

  ADNormalEventProvenance ne;
  ne.addNormalEvent(0,1,2,0, entry1);
  ne.addNormalEvent(100,101,102,100, entry2);
  
  constexpr bool add_outstanding = true;
  constexpr bool do_delete = true;

  //Check event retrieval without delete
  auto r1 = ne.getNormalEvent(0,1,2,0,!add_outstanding, !do_delete);
  EXPECT_EQ(r1.second, true);
  EXPECT_EQ(r1.first, entry1);

  r1 = ne.getNormalEvent(0,1,2,0,!add_outstanding, !do_delete);
  EXPECT_EQ(r1.second, true);
  EXPECT_EQ(r1.first, entry1);

  //Check retrieval with delete
  r1 = ne.getNormalEvent(0,1,2,0,!add_outstanding, do_delete);
  EXPECT_EQ(r1.second, true);
  EXPECT_EQ(r1.first, entry1);

  r1 = ne.getNormalEvent(0,1,2,0,!add_outstanding, do_delete);
  EXPECT_EQ(r1.second, false);
  
  //Check for second event
  auto r2 = ne.getNormalEvent(100,101,102,100, !add_outstanding, !do_delete);
  EXPECT_EQ(r2.second, true);
  EXPECT_EQ(r2.first, entry2);

  //Check for event that doesn't exist
  auto r3 = ne.getNormalEvent(200,201,202,200, add_outstanding, !do_delete);
  EXPECT_EQ(r3.second, false);

  //Ensure no outstanding events can yet be furnished
  auto out = ne.getOutstandingRequests(!do_delete);
  EXPECT_EQ(out.size(), 0);

  //Add the missing event
  ne.addNormalEvent(200,201,202,200, entry3);

  //Should now get the event as an outstanding request
  out = ne.getOutstandingRequests(do_delete);
  EXPECT_EQ(out.size(), 1);
  EXPECT_EQ(out[0], entry3);

  //Check it is returned only once
  out = ne.getOutstandingRequests(do_delete);
  EXPECT_EQ(out.size(), 0);

  //Check the outstanding request retrieval removed the internal copy
  r3 = ne.getNormalEvent(200,201,202,200, !add_outstanding, !do_delete);
  EXPECT_EQ(r3.second, false);

}
