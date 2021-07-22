#pragma once
#include <chimbuko_config.h>
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
     * @brief Signal a fatal error. Not written to output stream unless either uncaught (which will also call abort) or manually flushed
     */
    void fatal(const std::string &msg, const std::string &func, const std::string &file,  const unsigned long line);

    /**
     * @brief Manually flush an exception to the output stream
     */
    void flushError(std::exception_ptr e);
    void flushError(const std::exception &e);

  private:
    std::string getErrStr(const std::string &type, const std::string &msg, const std::string &func, const std::string &file,  const unsigned long line) const;

    int m_rank;
    std::ostream *m_ostream;
    std::mutex m_mutex;
  };

  /**
   * @brief The global error writer instance
   */
  ErrorWriter & Error();

  /**
   * @brief For fatal errors we delay writing the error to the output stream in case it is caught. This terminate handler ensures it is written
   *
   * After flushing the error the handler calls the terminateHandlerAbortAction above
   */
  void writeErrorTerminateHandler();

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
  { chimbuko::Error().recoverable(MSG, __func__, __FILE__, __LINE__); }

  /**
   * @brief Signal a fatal error
   */
#define fatal_error(MSG) \
  { chimbuko::Error().fatal(MSG, __func__, __FILE__, __LINE__); }

}
