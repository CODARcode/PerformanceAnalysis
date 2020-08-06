#include "gtest/gtest.h"
#include <chimbuko/util/commandLineParser.hpp>
#include <sstream>
using namespace chimbuko;


TEST(commandLineParserTest, Works){
  struct test{
    int a;
    std::string b;
    double cman;
  };
  
  commandLineParser<test> parser;
  parser.addMandatoryArg<double, &test::cman>("Provide the value for cman");
  parser.addOptionalArg<int, &test::a>("-a", "Provide the value of a");
  parser.addOptionalArg<std::string, &test::b>("-b", "Provide the value of b");
  

  int narg = 5;
  const char* args[] = { "3.14", "-a", "21", "-b", "Hello World" };
  
  test t;
  parser.parse(t, narg, args);
  EXPECT_EQ(t.a, 21);
  EXPECT_EQ(t.b, "Hello World");
}
TEST(commandLineParserTest, FailsIfCannotParse){
  struct test{
    int a;
  };
  
  commandLineParser<test> parser;
  parser.addOptionalArg<int, &test::a>("-a", "Provide the value of a");
  
  int narg = 2;
  const char* args[] = { "-a", "NOTANUMBER" };
  
  test t;
  EXPECT_THROW({
      parser.parse(t, narg, args);
    },
    std::runtime_error);
}

TEST(commandLineParserTest, FailsIfMandatorgArgNotProvided){
  struct test{
    int a;
  };
  
  commandLineParser<test> parser;
  parser.addMandatoryArg<int, &test::a>("Provide the value of a");
  
  int narg = 2;
  const char* args[] = { "-b", "I'm not a valid argument!" };
  
  test t;
  EXPECT_THROW({
      parser.parse(t, narg, args);
    },
    std::runtime_error);
}



TEST(commandLineParserTest, FailsIfInvalidArg){
  struct test{
    int a;
  };
  
  commandLineParser<test> parser;
  parser.addOptionalArg<int, &test::a>("-a", "Provide the value of a");
  
  int narg = 2;
  const char* args[] = { "-b", "I'm not a valid argument!" };
  
  test t;
  EXPECT_THROW({
      parser.parse(t, narg, args);
    },
    std::runtime_error);
}

TEST(commandLineParserTest, WorksWithMacro){
  struct test{
    int a;
    std::string b;
  };
  
  commandLineParser<test> parser;
  addOptionalCommandLineArg(parser, a, "Provide the value of a");
  addOptionalCommandLineArg(parser, b, "Provide the value of b");

  int narg = 4;
  const char* args[] = { "-a", "21", "-b", "Hello World" };
  
  test t;
  parser.parse(t, narg, args);
  EXPECT_EQ(t.a, 21);
  EXPECT_EQ(t.b, "Hello World");
}

TEST(commandLineParserTest, HelpStringWorks){
  struct test{
    int a;
    std::string b;
  };
  
  commandLineParser<test> parser;
  addOptionalCommandLineArg(parser, a, "Test string");

  std::stringstream ss;
  parser.help(ss);

  EXPECT_EQ(ss.str(), "Optional arguments:\n-a : Test string\n");
}

TEST(commandLineParserTest, DefaultHelpStringWorks){
  struct test{
    int a;
    std::string b;
  };
  
  commandLineParser<test> parser;
  addOptionalCommandLineArgDefaultHelpString(parser, a);

  std::stringstream ss;
  parser.help(ss);

  EXPECT_EQ(ss.str(), "Optional arguments:\n-a : Provide the value for a\n");
}





