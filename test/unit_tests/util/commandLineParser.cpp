#include "gtest/gtest.h"
#include <chimbuko/util/commandLineParser.hpp>
#include <sstream>
using namespace chimbuko;


TEST(commandLineParserTest, Works){
  struct test{
    int a;
    std::string b;
  };
  
  commandLineParser<test> parser;
  parser.addArg<int, &test::a>("-a", "Provide the value of a");
  parser.addArg<std::string, &test::b>("-b", "Provide the value of b");
  
  int narg = 4;
  const char* args[] = { "-a", "21", "-b", "Hello World" };
  
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
  parser.addArg<int, &test::a>("-a", "Provide the value of a");
  
  int narg = 2;
  const char* args[] = { "-a", "NOTANUMBER" };
  
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
  parser.addArg<int, &test::a>("-a", "Provide the value of a");
  
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
  addCommandLineArg(parser, a, "Provide the value of a");
  addCommandLineArg(parser, b, "Provide the value of b");

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
  addCommandLineArg(parser, a, "Test string");

  std::stringstream ss;
  parser.help(ss);

  EXPECT_EQ(ss.str(), "-a : Test string\n");
}

TEST(commandLineParserTest, DefaultHelpStringWorks){
  struct test{
    int a;
    std::string b;
  };
  
  commandLineParser<test> parser;
  addCommandLineArgDefaultHelpString(parser, a);

  std::stringstream ss;
  parser.help(ss);

  EXPECT_EQ(ss.str(), "-a : Provide the value for a\n");
}





