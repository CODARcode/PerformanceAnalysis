/** @file */
#pragma once

#include<vector>
#include<memory>
#include<string>
#include<type_traits>
#include<chimbuko/util/string.hpp>

namespace chimbuko{

  /**
   * @brief Base class for arg parsing structs
   */
  template<typename ArgsStruct>
  class commandLineArgBase{
  public:
    /**
     * @brief If the first string matches the internal arg string (eg "-help") parse the second string val and return true. If first string doesn't match or val is unable to be parsed, return false.
     */
    virtual bool parse(ArgsStruct &into, const std::string &arg, const std::string &val) = 0;
    /**
     * @brief Print the help string for this argument to the ostream
     */
    virtual void help(std::ostream &os) const = 0;

    virtual ~commandLineArgBase(){}
  };

  /**
   * @brief A class that parses an argument of a given type into the struct
   */
  template<typename ArgsStruct, typename T, T ArgsStruct::* P>
  class commandLineArg: public commandLineArgBase<ArgsStruct>{
  private:
    std::string m_arg; /**< The argument, format "-a" */
    std::string m_help_str; /**< The help string */
  public:
    /**
     * @brief Create an instance with the provided argument and help string
     */
    commandLineArg(const std::string &arg, const std::string &help_str): m_arg(arg), m_help_str(help_str){}
    
    bool parse(ArgsStruct &into, const std::string &arg, const std::string &val) override{
      if(arg == m_arg){
	T v;
	try{
	  v = strToAny<T>(val);
	}catch(const std::exception &exc){
	  return false;
	}
	into.*P = std::move(v);	
	return true;
      }
      return false;
    }
    void help(std::ostream &os) const override{
      os << m_arg << " : " << m_help_str;
    }
  };
  
  /**
   * @brief The main parser class for a generic struct ArgsStruct
   */
  template<typename ArgsStruct>
  class commandLineParser{
  private:
    std::vector<std::unique_ptr<commandLineArgBase<ArgsStruct> > > m_args; /**< Container for the individual arg parsers */
  public:
    typedef ArgsStruct StructType;

    /**
     * @brief Add an argument with the given type, member pointer (eg &ArgsStruct::a) with provided argument (eg "-a") and help string
     */
    template<typename T, T ArgsStruct::* P>
    void addArg(const std::string &arg, const std::string &help_str){
      m_args.emplace_back(new commandLineArg<ArgsStruct,T,P>(arg, help_str));
    }
    
    /**
     * @brief Parse an array of strings of length 'narg' into the structure
     */
    void parse(ArgsStruct &into, const int narg, const char** args){
      for(size_t i=0;i<narg;i+=2){
	std::string arg(args[i]), val(args[i+1]);
	bool success = false;
	for(size_t a=0;a<m_args.size();a++){
	  if(m_args[a]->parse(into, arg,val)){
	    success = true;
	    break;
	  }
	}
	if(!success) throw std::runtime_error("Could not parse argument pair:" + arg + " " + val);
      }
    }
    /**
     * @brief Print the help information for all the args that can be parsed
     */
    void help(std::ostream &os = std::cout) const{
      for(size_t a=0;a<m_args.size();a++){
	m_args[a]->help(os);
	os << std::endl;
      }
    }
  };

  /**@brief Helper macro to add a command line arg to the parser PARSER with given name NAME and help string HELP_STR */
#define addCommandLineArg(PARSER, NAME, HELP_STR) PARSER.addArg< decltype(std::decay<decltype(PARSER)>::type::StructType ::NAME) , &std::decay<decltype(PARSER)>::type::StructType ::NAME>("-" #NAME, HELP_STR)
  /**@brief Helper macro to add a command line arg to the parser PARSER with given name NAME and default help string "Provide the value for NAME" */
#define addCommandLineArgDefaultHelpString(PARSER, NAME) addCommandLineArg(PARSER, NAME, "Provide the value for " #NAME)


};
