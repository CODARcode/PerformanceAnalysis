#include "gtest/gtest.h"
#include <chimbuko/util/commandLineParser.hpp>
#include <sstream>
using namespace chimbuko;


TEST(commandLineParserTest, Works){
  struct test{
    int a;
    std::string b;
    double cman;

    std::string d;
    bool d_set;

    int e_val1;
    float e_val2;
    
    int f_val1;
    std::string f_val2;

    test(): d_set(false){}
  };
  
  commandLineParser<test> parser;
  parser.addMandatoryArg<double, &test::cman>("Provide the value for cman");
  parser.addOptionalArg<int, &test::a>("-a", "Provide the value of a");
  parser.addOptionalArg<std::string, &test::b>("-b", "Provide the value of b");
  parser.addOptionalArgWithFlag<std::string, &test::d, &test::d_set>("-d", "Provide the value of d");
  parser.addOptionalArgMultiValue<MemberPtrContainer<test,int, &test::e_val1>, MemberPtrContainer<test,float, &test::e_val2> >("-e", "Provide the value of e");  

  //Test the variadic macro
  addOptionalCommandLineArgMultiValue(parser, f, "Provide the value of f_val1 and f_val2", f_val1, f_val2);


  int narg = 13;
  const char* args[] = { "3.14", 
			 "-a", "21", 
			 "-b", "Hello World", 
			 "-d", "MyD", 
			 "-e", "99", "6.28",
			 "-f", "876", "evil macros"};
  
  test t;
  parser.parse(t, narg, args);
  EXPECT_FLOAT_EQ(t.cman, 3.14);
  EXPECT_EQ(t.a, 21);
  EXPECT_EQ(t.b, "Hello World");
  EXPECT_EQ(t.d, "MyD");
  EXPECT_EQ(t.d_set, true);
  EXPECT_EQ(t.e_val1, 99);
  EXPECT_FLOAT_EQ(t.e_val2, 6.28);
  EXPECT_EQ(t.f_val1, 876);
  EXPECT_EQ(t.f_val2, "evil macros");
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





