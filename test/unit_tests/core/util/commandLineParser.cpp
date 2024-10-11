#include "gtest/gtest.h"
#include <chimbuko/core/util/commandLineParser.hpp>
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
  
  test t;
  commandLineParser parser;
  parser.addMandatoryArg(t.cman,"Provide the value for cman");
  parser.addOptionalArg(t.a,"-a", "Provide the value of a");
  parser.addOptionalArg(t.b,"-b", "Provide the value of b");
  parser.addOptionalArgWithFlag(t.d, t.d_set, "-d", "Provide the value of d");
  parser.addOptionalArgMultiValue(std::string("-e"), std::string("Provide the value of e"), t.e_val1, t.e_val2);  

  //Test the variadic macro
  addOptionalCommandLineArgMultiValue(parser, t, f, "Provide the value of f_val1 and f_val2", f_val1, f_val2);


  int narg = 13;
  const char* args[] = { "3.14", 
			 "-a", "21", 
			 "-b", "Hello World", 
			 "-d", "MyD", 
			 "-e", "99", "6.28",
			 "-f", "876", "evil macros"};
  
  parser.parse(narg, args);
  EXPECT_FLOAT_EQ(t.cman, 3.14);
  EXPECT_EQ(t.a, 21);
  EXPECT_EQ(t.b, "Hello World");
  EXPECT_EQ(t.d, "MyD");
  EXPECT_EQ(t.d_set, true);
  ///EXPECT_EQ(t.e_val1, 99);
  //EXPECT_FLOAT_EQ(t.e_val2, 6.28);
  //EXPECT_EQ(t.f_val1, 876);
  //EXPECT_EQ(t.f_val2, "evil macros");
}
TEST(commandLinkParserTest, DerivedClassWorks){
  struct base{
    int a;
  };
  struct derived: public base{
    float b;
  };
  derived t;
  commandLineParser parser;
  parser.addOptionalArg(t.a, "-a", "Provide the value of a");
  parser.addOptionalArg(t.b, "-b", "Provide the value of b");

  int narg = 4;
  const char* args[] = { "-a", "21", 
			 "-b", "3.141"};
  
  parser.parse(narg, args);
  
  EXPECT_EQ(t.a, 21);
  EXPECT_FLOAT_EQ(t.b, 3.141);
} 


TEST(commandLineParserTest, FailsIfCannotParse){
  struct test{
    int a;
  };
  
  test t;
  commandLineParser parser;
  parser.addOptionalArg(t.a, "-a", "Provide the value of a");
  
  int narg = 2;
  const char* args[] = { "-a", "NOTANUMBER" };
  
  EXPECT_THROW({
      parser.parse(narg, args);
    },
    std::runtime_error);
}

TEST(commandLineParserTest, FailsIfMandatorgArgNotProvided){
  struct test{
    int a;
  };

  test t;
  commandLineParser parser;
  parser.addMandatoryArg(t.a,"Provide the value of a");
  
  int narg = 2;
  const char* args[] = { "-b", "I'm not a valid argument!" };
  
  EXPECT_THROW({
      parser.parse(narg, args);
    },
    std::runtime_error);
}



TEST(commandLineParserTest, FailsIfInvalidArg){
  struct test{
    int a;
  };
  
  test t;
  commandLineParser parser;
  parser.addOptionalArg(t.a, "-a", "Provide the value of a");
  
  int narg = 2;
  const char* args[] = { "-b", "I'm not a valid argument!" };
  
  EXPECT_THROW({
      parser.parse(narg, args);
    },
    std::runtime_error);
}

TEST(commandLineParserTest, WorksWithMacro){
  struct test{
    int a;
    std::string b;
  };
  
  test t;
  commandLineParser parser;
  addOptionalCommandLineArg(parser, t, a, "Provide the value of a");
  addOptionalCommandLineArg(parser, t, b, "Provide the value of b");

  int narg = 4;
  const char* args[] = { "-a", "21", "-b", "Hello World" };
  
  parser.parse(narg, args);
  EXPECT_EQ(t.a, 21);
  EXPECT_EQ(t.b, "Hello World");
}

TEST(commandLinkParserTest, DerivedClassWorksWithMacro){
  struct base{
    int a;
  };
  struct derived: public base{
    float b;
  };
  derived t;
  commandLineParser parser;
  addOptionalCommandLineArg(parser, t, a, "Provide the value of a");
  addOptionalCommandLineArg(parser, t, b, "Provide the value of b");

  int narg = 4;
  const char* args[] = { "-a", "21", 
			 "-b", "3.141"};
 
  parser.parse(narg, args);
  
  EXPECT_EQ(t.a, 21);
  EXPECT_FLOAT_EQ(t.b, 3.141);
} 


TEST(commandLineParserTest, HelpStringWorks){
  struct test{
    int a;
    std::string b;
  };
  
  test t;
  commandLineParser parser;
  addOptionalCommandLineArg(parser, t, a, "Test string");

  std::stringstream ss;
  parser.help(ss);

  EXPECT_EQ(ss.str(), "Optional arguments:\n-a : Test string\n");
}

TEST(commandLineParserTest, DefaultHelpStringWorks){
  struct test{
    int a;
    std::string b;
  };
  
  test t;
  commandLineParser parser;
  addOptionalCommandLineArgDefaultHelpString(parser, t, a);

  std::stringstream ss;
  parser.help(ss);

  EXPECT_EQ(ss.str(), "Optional arguments:\n-a : Provide the value for a\n");
}





