#include<chimbuko/util/error.hpp>
#include<chimbuko/util/string.hpp>

#include <iostream>
#include <sstream>
#include <fstream>


namespace chimbuko{

  ErrorWriter::ErrorWriter(): m_rank(-1), m_ostream(&std::cerr){}
  
  void ErrorWriter::recoverable(const std::string &msg, const std::string &func, const std::string &file,  const unsigned long line){
    std::lock_guard<std::mutex> _(m_mutex);
    *m_ostream << getErrStr("recoverable", msg, func, file, line);
  }
  void ErrorWriter::fatal(const std::string &msg, const std::string &func, const std::string &file,  const unsigned long line){
    std::string err = getErrStr("FATAL", msg, func, file, line);      
    std::lock_guard<std::mutex> _(m_mutex);
    *m_ostream << err;
    throw std::runtime_error(err);
  }

  std::string ErrorWriter::getErrStr(const std::string &type, const std::string &msg, const std::string &func, 
				     const std::string &file,  const unsigned long line) const{
    std::stringstream ss;
    ss<< "Error (" << type << ")";
    if(m_rank != -1) ss << " rank " << m_rank;
    ss << " : " << func << " (" << file << ":" << line << ") : " << msg <<std::endl;
    return ss.str();
  }

  ErrorWriter g_error;

  void set_error_output_stream(const int rank, std::ostream *strm){
    g_error.setRank(rank);
    g_error.setStream(strm);
  }

  void set_error_output_file(const int rank, const std::string &file_stub){
    static std::ofstream f(stringize("%s.%d",file_stub.c_str(),rank));
    g_error.setRank(rank);
    g_error.setStream(&f);
  }



}
