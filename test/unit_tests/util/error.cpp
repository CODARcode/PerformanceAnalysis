#include <sstream>
#include "chimbuko/util/error.hpp"
#include "gtest/gtest.h"

using namespace chimbuko;

std::string removeDateTime(const std::string &from){
  size_t pos = from.find_first_of(']');
  if(pos == std::string::npos) return from;
  return from.substr(pos+2);
}
  

TEST(TestError, runTests){
  //Test with default rank
  {
    std::stringstream ss;
    g_error.setStream(&ss);
    g_error.recoverable("hello", "test_func", "this_file", 129);

    std::string e = removeDateTime(ss.str());
    EXPECT_EQ(e, "Error (recoverable) : test_func (this_file:129) : hello\n");
  }

  //Test with specific rank
  {
    std::stringstream ss;
    set_error_output_stream(22, &ss);
    g_error.recoverable("hello", "test_func", "this_file", 129);

    std::string e = removeDateTime(ss.str());
    EXPECT_EQ(e, "Error (recoverable) rank 22 : test_func (this_file:129) : hello\n");
  }

  //Test with auto source code lines
  {
    std::stringstream ss;
    set_error_output_stream(22, &ss);    
    unsigned long line = __LINE__ + 1;
    recoverable_error("hello");

    std::string e = removeDateTime(ss.str());
    
    std::stringstream expect;
    expect << "Error (recoverable) rank 22 : " << __func__ << " (" << __FILE__ << ":" << line <<") : hello\n";
    
    std::cout << e << std::endl;
    EXPECT_EQ(e, expect.str());
  }

  //Test fatal error with auto source code lines
  {
    std::stringstream ss;
    set_error_output_stream(22, &ss);
    bool caught = false;
    std::string thrown_err;
    unsigned long line = __LINE__ + 2;
    try{
      fatal_error("hello");
    }catch(std::exception &e){
      thrown_err = removeDateTime(e.what());
      caught = true;
    }

    EXPECT_EQ(caught, true);

    std::string e = removeDateTime(ss.str());

    std::stringstream expect;
    expect << "Error (FATAL) rank 22 : " << __func__ << " (" << __FILE__ << ":" << line <<") : hello\n";
    
    std::cout << e << std::endl;
    std::cout << thrown_err << std::endl;

    EXPECT_EQ(e, expect.str());
    EXPECT_EQ(thrown_err, expect.str());
  }





}
