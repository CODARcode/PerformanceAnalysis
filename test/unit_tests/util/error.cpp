#include <sstream>
#include "chimbuko/util/error.hpp"
#include "gtest/gtest.h"

using namespace chimbuko;

std::string removeDateTime(const std::string &from){
  size_t pos = from.find_first_of(']');
  if(pos == std::string::npos) return from;
  return from.substr(pos+2);
}

void donothing(){}

TEST(TestError, runTests){
  //Test with default rank
  {
    std::stringstream ss;
    Error().setStream(&ss);
    Error().recoverable("hello", "test_func", "this_file", 129);

    std::string e = removeDateTime(ss.str());
    EXPECT_EQ(e, "Error (recoverable) : test_func (this_file:129) : hello\n");
  }

  //Test with specific rank
  {
    std::stringstream ss;
    set_error_output_stream(22, &ss);
    Error().recoverable("hello", "test_func", "this_file", 129);

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
    unsigned long line = __LINE__ + 3;
    std::runtime_error thrown_err("pooh");
    try{
      fatal_error("hello");
    }catch(const std::runtime_error &e){
      std::cout << "Caught " << e.what() << std::endl;
      thrown_err = e;
      caught = true;
    }

    EXPECT_EQ(caught, true);

    std::stringstream expect_ss;
    expect_ss << "Error (FATAL) rank 22 : " << __func__ << " (" << __FILE__ << ":" << line <<") : hello";
    std::string expect = expect_ss.str();

    std::string thrown_err_str = removeDateTime(thrown_err.what());
    thrown_err_str = thrown_err_str.substr(0,expect.size()); //remove stack trace information

    std::cout << thrown_err_str << std::endl;
    EXPECT_EQ(thrown_err_str, expect);

    //Normally fatal errors are only written to the error stream if they are uncaught
    //As we caught the error above it will not be in the error stream unless we do an explicit flush
    EXPECT_EQ(ss.str().size(), 0);

    Error().flushError(thrown_err);

    std::string e = removeDateTime(ss.str());
    e = e.substr(0,expect.size());

    std::cout << e << std::endl;

    EXPECT_EQ(e, expect);
  }
}
