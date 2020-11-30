/** @file */
#pragma once
#include<iostream>
#include<vector>
#include<memory>
#include<string>
#include<type_traits>
#include<tuple>
#include<chimbuko/util/string.hpp>

namespace chimbuko{

  /**
   * @brief Base class for optional arg parsing structs
   */
  template<typename ArgsStruct>
  class optionalCommandLineArgBase{
  public:
    /**
     * @brief If the first string matches the internal arg string (eg "-help"), a number of strings are consumed from the array 'vals' and that number returned. 
     * A value of -1 indicates the argument did not match.
     *
     * @param into The output structure
     * @param vals An array of strings
     * @param vals_size The length of the string array
     */
    virtual int parse(ArgsStruct &into, const std::string &arg, const char** vals, const int vals_size) = 0;
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
    
    int parse(ArgsStruct &into, const std::string &arg, const char** vals, const int vals_size) override{
      if(arg == m_arg){
	if(vals_size < 1) return -1;

	T v;
	try{
	  v = strToAny<T>(vals[0]);
	}catch(const std::exception &exc){
	  return -1;
	}
	into.*P = std::move(v);	
	return 1;
      }
      return -1;
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
    
    int parse(ArgsStruct &into, const std::string &arg, const char** vals, const int vals_size) override{
      if(arg == m_arg){
	if(vals_size < 1) return -1;

	T v;
	try{
	  v = strToAny<T>(vals[0]);
	}catch(const std::exception &exc){
	  return -1;
	}
	into.*P = std::move(v);	
	into.*Flag = true;
	return 1;
      }
      return -1;
    }
    void help(std::ostream &os) const override{
      os << m_arg << " : " << m_help_str;
    }
  };


  /**
   * @brief A class containing a member function pointer
   */
  template<typename S, typename T, T S::* P>
  struct MemberPtrContainer{
    typedef T type;
    static constexpr T S::* value = P;
  };

  
  /**
   * @brief Recursive template class for parsing multiple values
   */
  template<typename ArgsStruct, typename TheMemberPtrContainer, class... RemainingMemberPtrContainers>
  struct optionalCommandLineArgMultiValue_parse{
    static void parse(ArgsStruct &into, const char** vals){
      auto v = strToAny<typename TheMemberPtrContainer::type>(vals[0]); //will throw if can't parse
      into.*TheMemberPtrContainer::value = std::move(v);	
      optionalCommandLineArgMultiValue_parse<ArgsStruct, RemainingMemberPtrContainers...>::parse(into, vals+1);
    }
  };
  template<typename ArgsStruct, typename TheMemberPtrContainer>
  struct optionalCommandLineArgMultiValue_parse<ArgsStruct, TheMemberPtrContainer>{
    static void parse(ArgsStruct &into, const char** vals){
      auto v = strToAny<typename TheMemberPtrContainer::type>(vals[0]); //will throw if can't parse
      into.*TheMemberPtrContainer::value = std::move(v);	
    }
  };
    


  /**
   * @brief A class that parses an argument of a given type into the struct with multiple values
   */
  template<typename ArgsStruct, class... MemberPtrContainers>
  class optionalCommandLineArgMultiValue: public optionalCommandLineArgBase<ArgsStruct>{
  private:
    std::string m_arg; /**< The argument, format "-a" */
    std::string m_help_str; /**< The help string */

  public:
    static constexpr int NValues = std::tuple_size<std::tuple<MemberPtrContainers...>>::value;
    
    /**
     * @brief Create an instance with the provided argument and help string
     */
    optionalCommandLineArgMultiValue(const std::string &arg, const std::string &help_str): m_arg(arg), m_help_str(help_str){}
    
    int parse(ArgsStruct &into, const std::string &arg, const char** vals, const int vals_size) override{
      if(arg == m_arg){
	if(vals_size < NValues) return -1;
	try{
	  optionalCommandLineArgMultiValue_parse<ArgsStruct, MemberPtrContainers...>::parse(into, vals);
	}catch(const std::exception &exc){
	  return -1;
	}
	return NValues;
      }
      return -1;
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
     * @brief Add an optional argument parser object. Assumes ownership of pointer
     */
    void addOptionalArg(optionalCommandLineArgBase<ArgsStruct>* arg_parser){
      m_opt_args.emplace_back(arg_parser);
    }


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
     * @brief Add an optional argument that has multiple associated values. Template parameters should be a list of specializations of MemberPtrContainer,
     * e.g  MemberPtrContainer<ArgsStruct, A, &ArgsStruct::a>, MemberPtrContainer<ArgsStruct, B, &ArgsStruct::b>
     */
    template<class... MemberPtrContainers>
    void addOptionalArgMultiValue(const std::string &arg, const std::string &help_str){
      m_opt_args.emplace_back(new optionalCommandLineArgMultiValue<ArgsStruct,MemberPtrContainers...>(arg, help_str));
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

      while(i < narg){
	std::string arg(args[i]);
	const char** vals = args+i+1;

	bool success = false;
	for(size_t a=0;a<m_opt_args.size();a++){
	  int consumed = m_opt_args[a]->parse(into, arg,vals, narg-i-1);
	  if(consumed >= 0){
	    success = true;
	    i+=consumed;
	    break;
	  }
	}
	if(!success) throw std::runtime_error("Could not parse argument:" + arg);
	++i; //always shift 1 for next token
      }
    }

    /**
     * @brief Parse the command line arguments into the structure
     *
     * Parsing will commence with second entry of argv
     */
    void parseCmdLineArgs(ArgsStruct &into, int argc, char** argv){
      parse(into, argc-1, (const char**)(argv+1));
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


  /**
   * @brief Macros for generating the structure list needed for addOptionalArgMultiArg
   */
#define _GMP_GET_TYPE(T) std::decay<decltype(T)>::type::StructType
#define _GMP_GET_MEMBER_TYPE(T, NAME) decltype( _GMP_GET_TYPE(T)::NAME )
#define _GMP_GET_MEMBER_PTR(T, NAME) & _GMP_GET_TYPE(T)::NAME

#define GET_MEMBER_PTR_CON(T, NAME) MemberPtrContainer< typename _GMP_GET_TYPE(T), _GMP_GET_MEMBER_TYPE(T,NAME), _GMP_GET_MEMBER_PTR(T,NAME) >

#define _GMP_GE_0(T, NAME)
#define _GMP_GE_1(T, NAME) GET_MEMBER_PTR_CON(T,NAME)
#define _GMP_GE_2(T, NAME, ...) GET_MEMBER_PTR_CON(T,NAME), _GMP_GE_1(T, __VA_ARGS__)
#define _GMP_GE_3(T, NAME, ...) GET_MEMBER_PTR_CON(T,NAME), _GMP_GE_2(T, __VA_ARGS__)
#define _GMP_GE_4(T, NAME, ...) GET_MEMBER_PTR_CON(T,NAME), _GMP_GE_3(T, __VA_ARGS__)
#define _GMP_GE_5(T, NAME, ...) GET_MEMBER_PTR_CON(T,NAME), _GMP_GE_4(T, __VA_ARGS__)

#define _GMP_GET_MACRO(_0,_1,_2,_3,_4,_5,NAME,...) NAME
#define GET_MEMBER_PTR_CONS(T,...) \
  _GMP_GET_MACRO(_0,__VA_ARGS__,_GMP_GE_5,_GMP_GE_4,_GMP_GE_3,_GMP_GE_2,_GMP_GE_1,_GMP_GE_0)(T,__VA_ARGS__)





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

  /**@brief Helper macro to add an optional command line arg to the parser PARSER with argument name  -${ARG_NAME} which sets multiple variables
   *
   * Supports up to 5 variables
   *
   * Example usage:  addOptionalCommandLineArgMultiValue(parser_instance, set_2vals, a, b)
   * called with -set_2vals 1 2   will set the structure members a and b to 1 and 2, respectively
   */
#define addOptionalCommandLineArgMultiValue(PARSER, ARG_NAME, HELP_STR, ...) PARSER.addOptionalArgMultiValue<GET_MEMBER_PTR_CONS(PARSER, __VA_ARGS__)>( "-" #ARG_NAME, HELP_STR )


  /**@brief Helper macro to add a mandatory command line arg to the parser PARSER with given name NAME and help string HELP_STR */
#define addMandatoryCommandLineArg(PARSER, NAME, HELP_STR) PARSER.addMandatoryArg< decltype(std::decay<decltype(PARSER)>::type::StructType ::NAME) \
											   , &std::decay<decltype(PARSER)>::type::StructType ::NAME>(HELP_STR)
  /**@brief Helper macro to add an optional command line arg to the parser PARSER with given name NAME and default help string "Provide the value for NAME" */
#define addMandatoryCommandLineArgDefaultHelpString(PARSER, NAME) addMandatoryCommandLineArg(PARSER, NAME, "Provide the value for " #NAME)


};
