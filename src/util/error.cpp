#include<chimbuko/util/error.hpp>
#include<chimbuko/util/string.hpp>
#include<chimbuko/util/time.hpp>

#include <iostream>
#include <sstream>
#include <fstream>
#include <exception>
#include <memory>

namespace chimbuko{

  ErrorWriter::ErrorWriter(): m_rank(-1), m_ostream(&std::cerr){}

  void ErrorWriter::recoverable(const std::string &msg, const std::string &func, const std::string &file,  const unsigned long line){
    std::lock_guard<std::mutex> _(m_mutex);
    *m_ostream << getErrStr("recoverable", msg, func, file, line);
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


}
