#pragma once

#include <iostream>
#include <mutex>

namespace chimbuko{

  /**
   * @brief A class for writing out errors to an output stream
   */
  struct ErrorWriter{
  public:
    ErrorWriter();
    
    /**
     * @brief Set the MPI rank. This will add the rank to the error output
     */
    void setRank(const int rank){ m_rank = rank; }

    /**
     * @brief Set the output stream
     */
    void setStream(std::ostream* strm){ m_ostream = strm; }

    /**
     * @brief Signal a recoverable error
     */
    void recoverable(const std::string &msg, const std::string &func, const std::string &file,  const unsigned long line);

    /**
     * @brief Signal a fatal error
     */
    void fatal(const std::string &msg, const std::string &func, const std::string &file,  const unsigned long line);

  private:
    std::string getErrStr(const std::string &type, const std::string &msg, const std::string &func, const std::string &file,  const unsigned long line) const;

    int m_rank;
    std::ostream *m_ostream;
    std::mutex m_mutex;
  };

  extern ErrorWriter g_error; /**< Global instance of ErrorWriter*/
  
  /**
   * @brief Set the error output of the global error writer to a stream and specify the rank
   */
  void set_error_output_stream(const int rank, std::ostream *strm = &std::cerr);

  /**
   * @brief Set the error output of the global error writer to a file and specify the rank
   * @param file_stub The file name "stub". The actual filename will be ${file_stub}.${rank}
   */
  void set_error_output_file(const int rank, const std::string &file_stub = "ad_error");

  /**
   * @brief Signal a recoverable error
   */
#define recoverable_error(MSG) \
  { g_error.recoverable(MSG, __func__, __FILE__, __LINE__); }

  /**
   * @brief Signal a fatal error
   */
#define fatal_error(MSG) \
  { g_error.fatal(MSG, __func__, __FILE__, __LINE__); }
 
}
