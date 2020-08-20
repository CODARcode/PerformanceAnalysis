/** @file */
#pragma once

#include<vector>
#include<memory>
#include<string>
#include<type_traits>
#include<chimbuko/util/string.hpp>

namespace chimbuko{

  /**
   * @brief Base class for optional arg parsing structs
   */
  template<typename ArgsStruct>
  class optionalCommandLineArgBase{
  public:
    /**
     * @brief If the first string matches the internal arg string (eg "-help") parse the second string val and return true. If first string doesn't match or val is unable to be parsed, return false.
     */
    virtual bool parse(ArgsStruct &into, const std::string &arg, const std::string &val) = 0;
    /**
     * @brief Print the help string for this argument to the ostream
     */
    virtual void help(std::ostream &os) const = 0;

    virtual ~optionalCommandLineArgBase(){}
  };

  /**
   * @brief A class that parses an argument of a given type into the struct
   */
  template<typename ArgsStruct, typename T, T ArgsStruct::* P>
  class optionalCommandLineArg: public optionalCommandLineArgBase<ArgsStruct>{
  private:
    std::string m_arg; /**< The argument, format "-a" */
    std::string m_help_str; /**< The help string */
  public:
    /**
     * @brief Create an instance with the provided argument and help string
     */
    optionalCommandLineArg(const std::string &arg, const std::string &help_str): m_arg(arg), m_help_str(help_str){}
    
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
   * @brief A class that parses an argument of a given type into the struct and sets a bool flag argument to true
   */
  template<typename ArgsStruct, typename T, T ArgsStruct::* P, bool ArgsStruct::* Flag>
  class optionalCommandLineArgWithFlag: public optionalCommandLineArgBase<ArgsStruct>{
  private:
    std::string m_arg; /**< The argument, format "-a" */
    std::string m_help_str; /**< The help string */
  public:
    /**
     * @brief Create an instance with the provided argument and help string
     */
    optionalCommandLineArgWithFlag(const std::string &arg, const std::string &help_str): m_arg(arg), m_help_str(help_str){}
    
    bool parse(ArgsStruct &into, const std::string &arg, const std::string &val) override{
      if(arg == m_arg){
	T v;
	try{
	  v = strToAny<T>(val);
	}catch(const std::exception &exc){
	  return false;
	}
	into.*P = std::move(v);	
	into.*Flag = true;
	return true;
      }
      return false;
    }
    void help(std::ostream &os) const override{
      os << m_arg << " : " << m_help_str;
    }
  };







  /**
   * @brief Base class for mandatory arg parsing structs
   */
  template<typename ArgsStruct>
  class mandatoryCommandLineArgBase{
  public:
    /**
     * @brief Parse the value into the struct. Return false val is unable to be parsed
     */
    virtual bool parse(ArgsStruct &into, const std::string &val) = 0;
    /**
     * @brief Print the help string for this argument to the ostream
     */
    virtual void help(std::ostream &os) const = 0;

    virtual ~mandatoryCommandLineArgBase(){}
  };

  /**
   * @brief A class that parses an argument of a given type into the struct
   */
  template<typename ArgsStruct, typename T, T ArgsStruct::* P>
  class mandatoryCommandLineArg: public mandatoryCommandLineArgBase<ArgsStruct>{
  private:
    std::string m_help_str; /**< The help string */
  public:
    /**
     * @brief Create an instance with the provided argument and help string
     */
    mandatoryCommandLineArg(const std::string &help_str): m_help_str(help_str){}
    
    bool parse(ArgsStruct &into, const std::string &val) override{
      T v;
      try{
	v = strToAny<T>(val);
      }catch(const std::exception &exc){
	return false;
      }
      into.*P = std::move(v);	
      return true;
    }
    void help(std::ostream &os) const override{
      os << m_help_str;
    }
  };



  
  /**
   * @brief The main parser class for a generic struct ArgsStruct
   */
  template<typename ArgsStruct>
  class commandLineParser{
  private:
    std::vector<std::unique_ptr<mandatoryCommandLineArgBase<ArgsStruct> > > m_man_args; /**< Container for the individual mandatory arg parsers */
    std::vector<std::unique_ptr<optionalCommandLineArgBase<ArgsStruct> > > m_opt_args; /**< Container for the individual optional arg parsers */
  public:
    typedef ArgsStruct StructType;

    /**
     * @brief Add an optional argument with the given type, member pointer (eg &ArgsStruct::a) with provided argument (eg "-a") and help string
     */
    template<typename T, T ArgsStruct::* P>
    void addOptionalArg(const std::string &arg, const std::string &help_str){
      m_opt_args.emplace_back(new optionalCommandLineArg<ArgsStruct,T,P>(arg, help_str));
    }


    /**
     * @brief Add an optional argument with the given type, member pointer (eg &ArgsStruct::a), a bool flag member pointer (eg &ArgsStruct::got_value), with provided argument (eg "-a") and help string
     */
    template<typename T, T ArgsStruct::* P, bool ArgsStruct::* Flag>
    void addOptionalArgWithFlag(const std::string &arg, const std::string &help_str){
      m_opt_args.emplace_back(new optionalCommandLineArgWithFlag<ArgsStruct,T,P,Flag>(arg, help_str));
    }

    /**
     * @brief Add an mandatory argument with the given type, member pointer (eg &ArgsStruct::a) and help string
     */
    template<typename T, T ArgsStruct::* P>
    void addMandatoryArg(const std::string &help_str){
      m_man_args.emplace_back(new mandatoryCommandLineArg<ArgsStruct,T,P>(help_str));
    }

    /**
     * @brief Get the number of mandatory arguments
     */
    size_t nMandatoryArgs() const{ return m_man_args.size(); }
    
    /**
     * @brief Parse an array of strings of length 'narg' into the structure
     *
     * Parsing will commence with first entry of args
     */
    void parse(ArgsStruct &into, const int narg, const char** args){
      if(narg < m_man_args.size()) throw std::runtime_error("Not enough arguments provided: expect " + anyToStr(m_man_args.size()) );
      size_t i=0;
      for(;i<m_man_args.size();i++){
	if(!m_man_args[i]->parse(into, args[i])) throw std::runtime_error("Could not parse mandatory argument " + anyToStr(i));
      }

      if( (narg - i) % 2 != 0) throw std::runtime_error("Could not parse optional arguments: remaining arguments (" + anyToStr(narg-i) +") not a multiple of 2!");

      for(;i<narg;i+=2){
	std::string arg(args[i]), val(args[i+1]);
	bool success = false;
	for(size_t a=0;a<m_opt_args.size();a++){
	  if(m_opt_args[a]->parse(into, arg,val)){
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
      if(m_man_args.size()>0)
	os << "Requires " << m_man_args.size() << " arguments:" << std::endl;
      for(size_t a=0;a<m_man_args.size();a++){
	os << a << " : ";
	m_man_args[a]->help(os);
	os << std::endl;
      }

      if(m_opt_args.size()>0)
	os << "Optional arguments:" << std::endl;

      for(size_t a=0;a<m_opt_args.size();a++){
	m_opt_args[a]->help(os);
	os << std::endl;
      }
    }
  };

  /**@brief Helper macro to add an optional command line arg to the parser PARSER with given name NAME and help string HELP_STR. Option enabled by "-NAME" on command line */
#define addOptionalCommandLineArg(PARSER, NAME, HELP_STR) PARSER.addOptionalArg< decltype(std::decay<decltype(PARSER)>::type::StructType ::NAME) \
										 , &std::decay<decltype(PARSER)>::type::StructType ::NAME>("-" #NAME, HELP_STR)
  /**@brief Helper macro to add an optional command line arg to the parser PARSER with given name NAME and default help string "Provide the value for NAME" */
#define addOptionalCommandLineArgDefaultHelpString(PARSER, NAME) addOptionalCommandLineArg(PARSER, NAME, "Provide the value for " #NAME)

  /**@brief Helper macro to add an optional command line arg to the parser PARSER with given name NAME and help string HELP_STR. Option enabled by "-NAME" on command line. A bool field FLAGNAME will be set to true if parsed */
#define addOptionalCommandLineArgWithFlag(PARSER, NAME, FLAGNAME, HELP_STR) PARSER.addOptionalArgWithFlag< decltype(std::decay<decltype(PARSER)>::type::StructType ::NAME), \
													   &std::decay<decltype(PARSER)>::type::StructType ::NAME, \
													   &std::decay<decltype(PARSER)>::type::StructType ::FLAGNAME \
													>("-" #NAME, HELP_STR)


  /**@brief Helper macro to add a mandatory command line arg to the parser PARSER with given name NAME and help string HELP_STR */
#define addMandatoryCommandLineArg(PARSER, NAME, HELP_STR) PARSER.addMandatoryArg< decltype(std::decay<decltype(PARSER)>::type::StructType ::NAME) \
											   , &std::decay<decltype(PARSER)>::type::StructType ::NAME>(HELP_STR)
  /**@brief Helper macro to add an optional command line arg to the parser PARSER with given name NAME and default help string "Provide the value for NAME" */
#define addMandatoryCommandLineArgDefaultHelpString(PARSER, NAME) addMandatoryCommandLineArg(PARSER, NAME, "Provide the value for " #NAME)


};
