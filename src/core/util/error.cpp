#include<chimbuko/core/util/error.hpp>
#include<chimbuko/core/util/string.hpp>
#include<chimbuko/core/util/time.hpp>

#include <iostream>
#include <sstream>
#include <fstream>
#include <exception>
#include <memory>

#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>
#include <cxxabi.h>

namespace chimbuko{

  ErrorWriter::ErrorWriter(): m_rank(-1), m_ostream(&std::cerr){}

  void ErrorWriter::recoverable(const std::string &msg, const std::string &func, const std::string &file,  const unsigned long line){
    std::lock_guard<std::mutex> _(m_mutex);
    std::string err = getErrStr("recoverable", msg, func, file, line);
    *m_ostream << err;
    m_ostream->flush();
    if(m_ostream != &std::cerr){
      std::cerr << err;
      std::cerr.flush();
    }
  }
  void ErrorWriter::fatal(const std::string &msg, const std::string &func, const std::string &file,  const unsigned long line){
    std::string err = getErrStr("FATAL", msg, func, file, line);    
    throw std::runtime_error(err);
  }

  std::string ErrorWriter::getErrStr(const std::string &type, const std::string &msg, const std::string &func,
				     const std::string &file,  const unsigned long line) const{
    std::stringstream ss;
    ss << '[' << getDateTime() << "] Error (" << type << ")";
    if(m_rank != -1) ss << " rank " << m_rank;
    ss << " : " << func << " (" << file << ":" << line << ") : " << msg <<std::endl;
    
    if(type == "FATAL"){
      ss << "Stack trace:\n";
      stacktrace(ss);
    }

    return ss.str();
  }

  void ErrorWriter::flushError(std::exception_ptr e){
    if(e){
      try{ //this is necessary to get the exception type
	rethrow_exception(e);
      }catch(const std::exception &ev){
	flushError(ev);
      }
    }
  }
  void ErrorWriter::flushError(const std::exception &e){
    std::lock_guard<std::mutex> _(m_mutex);
    (*m_ostream) << e.what();
    m_ostream->flush();
  }

  ErrorWriter & Error(){
    static bool first = true;
    static ErrorWriter e;

    //Register the terminate handler
    if(first){
      std::set_terminate(writeErrorTerminateHandler);
      first = false;
    }
    return e;
  }

  void set_error_output_stream(const int rank, std::ostream *strm){
    Error().setRank(rank);
    Error().setStream(strm);
  }

  std::unique_ptr<std::ofstream> & error_output_file(){ static std::unique_ptr<std::ofstream> u; return u; }

  void set_error_output_file(const int rank, const std::string &file_stub){
    std::string filename = stringize("%s.%d",file_stub.c_str(),rank);
    error_output_file().reset(new std::ofstream(filename));
    if(error_output_file()->fail()) throw std::runtime_error("set_error_output_file could not open file " + filename);
    Error().setRank(rank);
    Error().setStream(error_output_file().get());
  }

  void writeErrorTerminateHandler(){
    if( auto exc = std::current_exception() ) {
      Error().flushError(exc);
    }

    //Make sure to close the error output file first because abort does not destruct static objects
    if(error_output_file()) error_output_file()->close();

    std::abort();
  }


  //Based on stacktrace.h (c) 2008, Timo Bingmann from http://idlebox.net/  published under the WTFPL v2.0
  void stacktrace(std::ostream &out, unsigned int max_frames){
    // storage array for stack trace address data
    void* addrlist[max_frames+1];

    // retrieve current stack addresses
    int addrlen = backtrace(addrlist, sizeof(addrlist) / sizeof(void*));

    if (addrlen == 0) {
      out << "<empty, possibly corrupt>" << std::endl;
      return;
    }

    // resolve addresses into strings containing "filename(function+address)",
    // this array must be free()-ed
    char** symbollist = backtrace_symbols(addrlist, addrlen);

    // allocate string which will be filled with the demangled function name
    size_t funcnamesize = 256;
    char* funcname = (char*)malloc(funcnamesize);

    // iterate over the returned symbol lines. skip the first, it is the
    // address of this function.
    for (int i = 0; i < addrlen; i++){
      char *begin_name = 0, *begin_offset = 0, *end_offset = 0;

      // find parentheses and +address offset surrounding the mangled name:
      // ./module(function+0x15c) [0x8048a6d]
      for (char *p = symbollist[i]; *p; ++p)
	{
	  if (*p == '(')
	    begin_name = p;
	  else if (*p == '+')
	    begin_offset = p;
	  else if (*p == ')' && begin_offset) {
	    end_offset = p;
	    break;
	  }
	}

      if (begin_name && begin_offset && end_offset
	  && begin_name < begin_offset)
	{
	  *begin_name++ = '\0';
	  *begin_offset++ = '\0';
	  *end_offset = '\0';

	  // mangled name is now in [begin_name, begin_offset) and caller
	  // offset in [begin_offset, end_offset). now apply
	  // __cxa_demangle():

	  int status;
	  char* ret = abi::__cxa_demangle(begin_name,
					  funcname, &funcnamesize, &status);
	  if (status == 0) {
	    funcname = ret; // use possibly realloc()-ed string
	    out << "  " << symbollist[i] << " : " << funcname << "+" << begin_offset << std::endl;
	  }
	  else {
	    // demangling failed. Output function name as a C function with
	    // no arguments.
	    out << "  " << symbollist[i] << " : " << begin_name << "+" << begin_offset << std::endl;
	  }
	}
      else
	{
	  // couldn't parse the line? print the whole line.
	  out << "  " << symbollist[i] << std::endl;
	}
    }

    free(funcname);
    free(symbollist);
  }        


}
