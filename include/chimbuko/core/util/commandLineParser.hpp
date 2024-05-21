/** @file */
#pragma once
#include <chimbuko_config.h>
#include<iostream>
#include<vector>
#include<memory>
#include<string>
#include<type_traits>
#include<tuple>
#include<chimbuko/core/util/string.hpp>

namespace chimbuko{

  /**
     @brief Template wizardry to get class type of a member pointer
  */
  template<class T> struct get_member_ptr_class;
  template<class T, class R> struct get_member_ptr_class<R T::*> { using type = T; };

  /**
     @brief Template wizardry to get data type of a member pointer
  */
  template<class T> struct get_member_ptr_data_type;
  template<class T, class R> struct get_member_ptr_data_type<R T::*> { using type = R; };


  /**
   * @brief Base class for optional arg parsing structs
   */
  class optionalCommandLineArgBase{
  public:
    /**
     * @brief If the first string matches the internal arg string (eg "-help"), a number of strings are consumed from the array 'vals' and that number returned. 
     * A value of -1 indicates the argument did not match.
     *
     * @param vals An array of strings
     * @param vals_size The length of the string array
     */
    virtual int parse(const std::string &arg, const char** vals, const int vals_size) = 0;
    /**
     * @brief Print the help string for this argument to the ostream
     */
    virtual void help(std::ostream &os) const = 0;

    virtual ~optionalCommandLineArgBase(){}
  };

  /**
   * @brief A class that parses an argument of a given type into the struct
   */
  template<typename MemberType>
  class optionalCommandLineArg: public optionalCommandLineArgBase{
  private:
    std::string m_arg; /**< The argument, format "-a" */
    std::string m_help_str; /**< The help string */
    MemberType &member;
  public:
    /**
     * @brief Create an instance with the provided argument and help string
     */
    optionalCommandLineArg(MemberType &member, const std::string &arg, const std::string &help_str): m_arg(arg), m_help_str(help_str), member(member){}
    
    int parse(const std::string &arg, const char** vals, const int vals_size) override{
      if(arg == m_arg){
	if(vals_size < 1) return -1;

	try{
	  member = strToAny<MemberType>(vals[0]);
	}catch(const std::exception &exc){
	  return -1;
	}
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
  template<typename MemberType>
  class optionalCommandLineArgWithFlag: public optionalCommandLineArgBase{
  private:
    std::string m_arg; /**< The argument, format "-a" */
    std::string m_help_str; /**< The help string */
    MemberType &member;
    bool &flag;
  public:
    /**
     * @brief Create an instance with the provided argument and help string
     */
    optionalCommandLineArgWithFlag(MemberType &member, bool &flag, const std::string &arg, const std::string &help_str): m_arg(arg), m_help_str(help_str), member(member), flag(flag){}
    
    int parse(const std::string &arg, const char** vals, const int vals_size) override{
      if(arg == m_arg){
	if(vals_size < 1) return -1;

	try{
	  member = strToAny<MemberType>(vals[0]);
	}catch(const std::exception &exc){
	  return -1;
	}
	flag = true;
	return 1;
      }
      return -1;
    }
    void help(std::ostream &os) const override{
      os << m_arg << " : " << m_help_str;
    }
  };


  /**
   * @brief Recursive template class for parsing multiple values
   */
  template<typename MemberType, class... RemainingMemberTypes>
  struct optionalCommandLineArgMultiValue_parse: public optionalCommandLineArgMultiValue_parse<RemainingMemberTypes...>{
    MemberType &member;
    
    optionalCommandLineArgMultiValue_parse(MemberType &member, RemainingMemberTypes&... rem): member(member), optionalCommandLineArgMultiValue_parse<RemainingMemberTypes...>(rem...){}

    void parse(const char** vals){
      member = strToAny<MemberType>(vals[0]); //will throw if can't parse
      this->optionalCommandLineArgMultiValue_parse<RemainingMemberTypes...>::parse(vals+1);
    }
  };
  template<typename MemberType>
  struct optionalCommandLineArgMultiValue_parse<MemberType>{
    MemberType &member;

    optionalCommandLineArgMultiValue_parse(MemberType &member): member(member){}

    void parse(const char** vals){
      member = strToAny<MemberType>(vals[0]); //will throw if can't parse
    }
  };
    


  /**
   * @brief A class that parses an argument of a given type into the struct with multiple values
   */
  template<class... MemberTypes>
  class optionalCommandLineArgMultiValue: public optionalCommandLineArgBase{
  private:
    std::string m_arg; /**< The argument, format "-a" */
    std::string m_help_str; /**< The help string */
    optionalCommandLineArgMultiValue_parse<MemberTypes...> pp;
  public:
    static constexpr int NValues = std::tuple_size<std::tuple<MemberTypes...>>::value;
    
    /**
     * @brief Create an instance with the provided argument and help string
     */
    optionalCommandLineArgMultiValue(MemberTypes&... members, const std::string &arg, const std::string &help_str): m_arg(arg), m_help_str(help_str), pp(members...){}
    
    int parse(const std::string &arg, const char** vals, const int vals_size) override{
      if(arg == m_arg){
  	if(vals_size < NValues) return -1;
  	try{
	  pp.parse(vals);
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
  class mandatoryCommandLineArgBase{
  public:
    /**
     * @brief Parse the value into the struct. Return false val is unable to be parsed
     */
    virtual bool parse(const std::string &val) = 0;
    /**
     * @brief Print the help string for this argument to the ostream
     */
    virtual void help(std::ostream &os) const = 0;

    virtual ~mandatoryCommandLineArgBase(){}
  };

  /**
   * @brief A class that parses an argument of a given type into the struct
   */
  template<typename MemberType>
  class mandatoryCommandLineArg: public mandatoryCommandLineArgBase{
  private:
    std::string m_help_str; /**< The help string */
    MemberType &member;
  public:
    /**
     * @brief Create an instance with the provided argument and help string
     */
    mandatoryCommandLineArg(MemberType &member, const std::string &help_str): m_help_str(help_str), member(member){}
    
    bool parse(const std::string &val) override{
      try{
	member = strToAny<MemberType>(val);
      }catch(const std::exception &exc){
	return false;
      }
      return true;
    }
    void help(std::ostream &os) const override{
      os << m_help_str;
    }
  };
  
  /**
   * @brief The main parser class for a generic struct ArgsStruct
   */
  class commandLineParser{
  private:
    std::vector<std::unique_ptr<mandatoryCommandLineArgBase> > m_man_args; /**< Container for the individual mandatory arg parsers */
    std::vector<std::unique_ptr<optionalCommandLineArgBase> > m_opt_args; /**< Container for the individual optional arg parsers */
  public:

    /**
     * @brief Add an optional argument parser object. Assumes ownership of pointer
     */
    void addOptionalArg(optionalCommandLineArgBase* arg_parser){
      m_opt_args.emplace_back(arg_parser);
    }


    /**
     * @brief Add an optional argument bound to the given member object with provided argument (eg "-a") and help string
     */
    template<typename MemberType>
    void addOptionalArg(MemberType &member, const std::string &arg, const std::string &help_str){
      m_opt_args.emplace_back(new optionalCommandLineArg<MemberType>(member,arg, help_str));
    }


    /**
     * @brief Add an optional argument bound to the given member object and a bool flag, with provided argument (eg "-a") and help string
     */
    template<typename MemberType>
    void addOptionalArgWithFlag(MemberType &member, bool &flag, const std::string &arg, const std::string &help_str){
      m_opt_args.emplace_back(new optionalCommandLineArgWithFlag<MemberType>(member, flag, arg, help_str));
    }


    /**
     * @brief Add an optional argument that has multiple associated values
     */
    template<class... MemberTypes>
    void addOptionalArgMultiValue(const std::string &arg, const std::string &help_str, MemberTypes&... members){
      m_opt_args.emplace_back(new optionalCommandLineArgMultiValue<MemberTypes...>(members..., arg, help_str));
    }

    /**
     * @brief Add an mandatory argument with the given type, member pointer (eg &ArgsStruct::a) and help string
     */
    template<typename MemberType>
    void addMandatoryArg(MemberType &member, const std::string &help_str){
      m_man_args.emplace_back(new mandatoryCommandLineArg<MemberType>(member,help_str));
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
    void parse(const int narg, const char** args){
      if(narg < m_man_args.size()) throw std::runtime_error("Not enough arguments provided: expect " + anyToStr(m_man_args.size()) );
      size_t i=0;
      for(;i<m_man_args.size();i++){
	if(!m_man_args[i]->parse(args[i])) throw std::runtime_error("Could not parse mandatory argument " + anyToStr(i));
      }

      while(i < narg){
	std::string arg(args[i]);
	const char** vals = args+i+1;

	bool success = false;
	for(size_t a=0;a<m_opt_args.size();a++){
	  int consumed = m_opt_args[a]->parse(arg,vals, narg-i-1);
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
    void parseCmdLineArgs(int argc, char** argv){
      parse(argc-1, (const char**)(argv+1));
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
#define _WRP_P_0(PARENT, CHILD)
#define _WRP_P_1(PARENT, CHILD) PARENT.CHILD
#define _WRP_P_2(PARENT, CHILD, ...) PARENT.CHILD, _WRP_P_1(PARENT, __VA_ARGS__)
#define _WRP_P_3(PARENT, CHILD, ...) PARENT.CHILD, _WRP_P_2(PARENT, __VA_ARGS__)
#define _WRP_P_4(PARENT, CHILD, ...) PARENT.CHILD, _WRP_P_3(PARENT, __VA_ARGS__)
#define _WRP_P_5(PARENT, CHILD, ...) PARENT.CHILD, _WRP_P_4(PARENT, __VA_ARGS__)

#define _WRP_P_GET_MACRO(_0,_1,_2,_3,_4,_5,NAME,...) NAME
#define WRAP_PARENT(PARENT,...)					\
  _WRP_P_GET_MACRO(_0,__VA_ARGS__,_WRP_P_5,_WRP_P_4,_WRP_P_3,_WRP_P_2,_WRP_P_1,_WRP_P_0)(PARENT,__VA_ARGS__)





  /**@brief Helper macro to add an optional command line arg to the parser PARSER for member with given name NAME, PARENT struct instance, and help string HELP_STR. Option enabled by "-NAME" on command line */
#define addOptionalCommandLineArg(PARSER, PARENT, NAME, HELP_STR) PARSER.addOptionalArg(PARENT.NAME, "-" #NAME, HELP_STR)

  /**@brief Helper macro to add an optional command line arg to the parser PARSER for member with given name NAME default value DEFAULT, PARENT struct instance, and help string HELP_STR. Option enabled by "-NAME" on command line */
#define addOptionalCommandLineArgWithDefault(PARSER, PARENT, NAME, DEFAULT, HELP_STR) PARSER.addOptionalArg(PARENT.NAME=DEFAULT, "-" #NAME, HELP_STR)

  /**@brief Helper macro to add an optional command line arg to the parser PARSER for member with given name NAME, PARENT struct instance, and default help string "Provide the value for NAME" */
#define addOptionalCommandLineArgDefaultHelpString(PARSER, PARENT, NAME) addOptionalCommandLineArg(PARSER, PARENT, NAME, "Provide the value for " #NAME)

  /**@brief Helper macro to add an optional command line arg to the parser PARSER for member with given name NAME and flag with FLAGNAME both belonging to PARENT struct instance, and help string HELP_STR. Option enabled by "-NAME" on command line. FLAGNAME will be set true if parsed */
#define addOptionalCommandLineArgWithFlag(PARSER, PARENT, NAME, FLAGNAME, HELP_STR) PARSER.addOptionalArgWithFlag(PARENT.NAME, PARENT.FLAGNAME, "-" #NAME, HELP_STR)

  /**@brief Helper macro to add an optional command line arg to the parser PARSER with argument name  -${ARG_NAME} which sets multiple variables
   *
   * Macro supports up to 5 members
   *
   * Example usage:  addOptionalCommandLineArgMultiValue(parser_instance, struct_instance, set_2vals, "the help", a, b)
   * called with -set_2vals 1 2   will set structure_instance members a and b to 1 and 2, respectively
   */
#define addOptionalCommandLineArgMultiValue(PARSER, PARENT, ARG_NAME, HELP_STR, ...) PARSER.addOptionalArgMultiValue( "-" #ARG_NAME, HELP_STR, WRAP_PARENT(PARENT, __VA_ARGS__) )

  /**@brief Helper macro to add an optional command line arg to the parser PARSER with given member name NAME, PARENT struct instance, and help string HELP_STR. Option enabled by "-OPT_ARG" on command line */
#define addOptionalCommandLineArgOptArg(PARSER, PARENT, NAME, OPT_ARG, HELP_STR) PARSER.addOptionalArg(PARENT.NAME, "-" #OPT_ARG, HELP_STR)

  /**@brief Helper macro to add an optional command line arg to the parser PARSER with given member name NAME, default value DEFAULT, PARENT struct instance, and help string HELP_STR. Option enabled by "-OPT_ARG" on command line */
#define addOptionalCommandLineArgOptArgWithDefault(PARSER, PARENT, NAME, OPT_ARG, DEFAULT, HELP_STR) PARSER.addOptionalArg(PARENT.NAME=DEFAULT, "-" #OPT_ARG, HELP_STR)



  /**@brief Helper macro to add a mandatory command line arg to the parser PARSER with given member name NAME, PARENT struct instance, and help string HELP_STR */
#define addMandatoryCommandLineArg(PARSER, PARENT, NAME, HELP_STR) PARSER.addMandatoryArg(PARENT.NAME, HELP_STR)
  /**@brief Helper macro to add an optional command line arg to the parser PARSER with given member name NAME, PARENT struct instance, and default help string "Provide the value for NAME" */
#define addMandatoryCommandLineArgDefaultHelpString(PARSER, PARENT, NAME) addMandatoryCommandLineArg(PARSER, PARENT.NAME, "Provide the value for " #NAME)


};
